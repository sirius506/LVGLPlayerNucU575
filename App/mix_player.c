/**
 *  Sound Mixer and Music player task
 */
#include "DoomPlayer.h"
#include "audio_output.h"
#include <arm_math.h>
#include <stdlib.h>
#include <stdint.h>
#include "fft.h"
#include "fatfs.h"
#include "app_music.h"

#define DR_FLAC_IMPLEMENTATION
#define	DR_FLAC_NO_CRC
#define	DR_FLAC_NO_STDIO
#define	DR_FLAC_NO_SIMD
#define	DR_FLAC_NO_OGG

#define	IDLE_PERIOD	50

#include "dr_flac.h"

#define PLAYER_STACK_SIZE  350
#define READER_STACK_SIZE  1400

SECTION_SRDSRAM AUDIO_STEREO FinalAudioBuffer[BUF_FRAMES];

static void mix_half_comp();
static void mix_full_comp();

const AUDIO_INIT_PARAMS doom_audio_params = {
  .buffer = FinalAudioBuffer,
  .buffer_size = sizeof(FinalAudioBuffer),
  .volume = AUDIO_DEF_VOL,
  .sample_rate = 44100,
  .fft_magdiv = 3,
  .txhalf_comp = mix_half_comp,
  .txfull_comp = mix_full_comp,
};

TASK_DEF(mixplayer, PLAYER_STACK_SIZE, osPriorityAboveNormal2)
TASK_DEF(flacreader, READER_STACK_SIZE, osPriorityNormal)

#define MIXREADQ_DEPTH     3

enum {
  READER_START = 1,
  READER_READ,
  READER_STOP,
};

static uint8_t readqBuffer[MIXREADQ_DEPTH * sizeof(uint16_t)];

static AUDIO_STEREO *musicqBuffer[BUF_FACTOR];
static void mix_request_data(int full);

MESSAGEQ_DEF(flacreadq, readqBuffer, sizeof(readqBuffer))
MESSAGEQ_DEF(musicbufq, musicqBuffer, sizeof(musicqBuffer))

Mix_Music *Mix_LoadMUS(const char *file);
int Mix_PlayMusic(Mix_Music *music, int loop);

/*
 * Buffers to store music source data.
 * Decoded FLAC music data is read into MusicFrameBuffer.
 * SilentBuffer is used to generate silience.
 */
SECTION_SRDSRAM static AUDIO_STEREO MusicFrameBuffer[BUF_FRAMES];
static const AUDIO_STEREO SilentBuffer[BUF_FRAMES];

#define	NUMTAPS	31

SECTION_SRDSRAM int16_t   sInBuffer[NUM_FRAMES];
SECTION_SRDSRAM float32_t sFloatBuffer[NUM_FRAMES];
SECTION_SRDSRAM float32_t DeciStateBuffer[NUM_FRAMES+NUMTAPS-1];

float32_t audio_buffer[AUDIO_SAMPLES];	// Decimated Audio samples

/*
 * Buffers for FFT process
 * Note that result buffer contains complex numbers, but its size is same as
 * input butter, because arm_rfft_fast_f32() process takes advantage of the
 * symmetry properties of the FFT.
 */
#if FFT_SAMPLES == 1024
static const float32_t hanning_window[FFT_SAMPLES] = {
  0.000000, 0.000009, 0.000038, 0.000085, 0.000151, 0.000235, 0.000339, 0.000461, 
  0.000602, 0.000762, 0.000941, 0.001138, 0.001355, 0.001590, 0.001844, 0.002116, 
  0.002408, 0.002718, 0.003047, 0.003394, 0.003760, 0.004145, 0.004549, 0.004971, 
  0.005412, 0.005871, 0.006349, 0.006846, 0.007361, 0.007895, 0.008447, 0.009018, 
  0.009607, 0.010215, 0.010841, 0.011486, 0.012149, 0.012830, 0.013530, 0.014248, 
  0.014984, 0.015739, 0.016512, 0.017303, 0.018112, 0.018939, 0.019785, 0.020648, 
  0.021530, 0.022429, 0.023347, 0.024282, 0.025236, 0.026207, 0.027196, 0.028203, 
  0.029228, 0.030270, 0.031330, 0.032408, 0.033504, 0.034617, 0.035747, 0.036895, 
  0.038060, 0.039243, 0.040443, 0.041660, 0.042895, 0.044147, 0.045416, 0.046702, 
  0.048005, 0.049326, 0.050663, 0.052017, 0.053388, 0.054776, 0.056180, 0.057601, 
  0.059039, 0.060494, 0.061965, 0.063452, 0.064956, 0.066477, 0.068014, 0.069567, 
  0.071136, 0.072721, 0.074322, 0.075940, 0.077573, 0.079222, 0.080888, 0.082569, 
  0.084265, 0.085977, 0.087705, 0.089449, 0.091208, 0.092982, 0.094771, 0.096576, 
  0.098396, 0.100231, 0.102082, 0.103947, 0.105827, 0.107722, 0.109631, 0.111556, 
  0.113495, 0.115448, 0.117416, 0.119399, 0.121396, 0.123407, 0.125432, 0.127471, 
  0.129524, 0.131592, 0.133673, 0.135768, 0.137876, 0.139999, 0.142135, 0.144284, 
  0.146447, 0.148623, 0.150812, 0.153014, 0.155230, 0.157458, 0.159699, 0.161954, 
  0.164220, 0.166500, 0.168792, 0.171097, 0.173414, 0.175743, 0.178084, 0.180438, 
  0.182803, 0.185181, 0.187570, 0.189971, 0.192384, 0.194809, 0.197244, 0.199692, 
  0.202150, 0.204620, 0.207101, 0.209593, 0.212096, 0.214610, 0.217134, 0.219669, 
  0.222215, 0.224771, 0.227337, 0.229914, 0.232501, 0.235098, 0.237705, 0.240322, 
  0.242949, 0.245585, 0.248231, 0.250886, 0.253551, 0.256225, 0.258908, 0.261600, 
  0.264302, 0.267012, 0.269731, 0.272458, 0.275194, 0.277939, 0.280692, 0.283453, 
  0.286222, 0.289000, 0.291785, 0.294578, 0.297379, 0.300188, 0.303004, 0.305827, 
  0.308658, 0.311496, 0.314341, 0.317193, 0.320052, 0.322918, 0.325791, 0.328670, 
  0.331555, 0.334447, 0.337345, 0.340249, 0.343159, 0.346075, 0.348997, 0.351924, 
  0.354858, 0.357796, 0.360740, 0.363689, 0.366643, 0.369603, 0.372567, 0.375536, 
  0.378510, 0.381488, 0.384471, 0.387458, 0.390449, 0.393445, 0.396444, 0.399448, 
  0.402455, 0.405466, 0.408480, 0.411498, 0.414519, 0.417543, 0.420571, 0.423601, 
  0.426635, 0.429671, 0.432709, 0.435751, 0.438795, 0.441841, 0.444889, 0.447939, 
  0.450991, 0.454045, 0.457101, 0.460159, 0.463218, 0.466278, 0.469339, 0.472402, 
  0.475466, 0.478531, 0.481596, 0.484662, 0.487729, 0.490796, 0.493864, 0.496932, 
  0.500000, 0.503068, 0.506136, 0.509203, 0.512270, 0.515337, 0.518403, 0.521469, 
  0.524534, 0.527597, 0.530660, 0.533722, 0.536782, 0.539841, 0.542898, 0.545954, 
  0.549008, 0.552061, 0.555111, 0.558159, 0.561205, 0.564249, 0.567290, 0.570329, 
  0.573365, 0.576398, 0.579429, 0.582456, 0.585481, 0.588502, 0.591520, 0.594534, 
  0.597545, 0.600552, 0.603556, 0.606555, 0.609550, 0.612542, 0.615529, 0.618512, 
  0.621490, 0.624464, 0.627433, 0.630397, 0.633356, 0.636311, 0.639260, 0.642204, 
  0.645142, 0.648075, 0.651003, 0.653925, 0.656841, 0.659751, 0.662655, 0.665553, 
  0.668445, 0.671330, 0.674209, 0.677082, 0.679947, 0.682806, 0.685658, 0.688504, 
  0.691342, 0.694172, 0.696996, 0.699812, 0.702620, 0.705421, 0.708215, 0.711000, 
  0.713777, 0.716547, 0.719308, 0.722061, 0.724805, 0.727542, 0.730269, 0.732988, 
  0.735698, 0.738399, 0.741092, 0.743775, 0.746449, 0.749114, 0.751769, 0.754415, 
  0.757051, 0.759678, 0.762295, 0.764902, 0.767499, 0.770086, 0.772662, 0.775229, 
  0.777785, 0.780331, 0.782866, 0.785390, 0.787904, 0.790407, 0.792899, 0.795380, 
  0.797849, 0.800308, 0.802755, 0.805191, 0.807616, 0.810028, 0.812430, 0.814819, 
  0.817196, 0.819562, 0.821916, 0.824257, 0.826586, 0.828903, 0.831208, 0.833500, 
  0.835779, 0.838046, 0.840300, 0.842542, 0.844770, 0.846986, 0.849188, 0.851377, 
  0.853553, 0.855716, 0.857865, 0.860001, 0.862123, 0.864232, 0.866327, 0.868408, 
  0.870475, 0.872529, 0.874568, 0.876593, 0.878604, 0.880601, 0.882583, 0.884552, 
  0.886505, 0.888444, 0.890368, 0.892278, 0.894173, 0.896053, 0.897918, 0.899768, 
  0.901604, 0.903424, 0.905228, 0.907018, 0.908792, 0.910551, 0.912295, 0.914022, 
  0.915735, 0.917431, 0.919112, 0.920777, 0.922427, 0.924060, 0.925677, 0.927279, 
  0.928864, 0.930433, 0.931986, 0.933523, 0.935043, 0.936547, 0.938035, 0.939506, 
  0.940961, 0.942398, 0.943820, 0.945224, 0.946612, 0.947983, 0.949337, 0.950674, 
  0.951995, 0.953298, 0.954584, 0.955853, 0.957105, 0.958339, 0.959557, 0.960757, 
  0.961940, 0.963105, 0.964253, 0.965383, 0.966496, 0.967592, 0.968669, 0.969729, 
  0.970772, 0.971797, 0.972804, 0.973793, 0.974764, 0.975717, 0.976653, 0.977571, 
  0.978470, 0.979352, 0.980215, 0.981061, 0.981888, 0.982697, 0.983488, 0.984261, 
  0.985016, 0.985752, 0.986470, 0.987170, 0.987851, 0.988514, 0.989159, 0.989785, 
  0.990393, 0.990982, 0.991553, 0.992105, 0.992639, 0.993154, 0.993651, 0.994129, 
  0.994588, 0.995029, 0.995451, 0.995855, 0.996240, 0.996606, 0.996953, 0.997282, 
  0.997592, 0.997884, 0.998156, 0.998410, 0.998645, 0.998862, 0.999059, 0.999238, 
  0.999398, 0.999539, 0.999661, 0.999765, 0.999849, 0.999915, 0.999962, 0.999991, 
  1.000000, 0.999991, 0.999962, 0.999915, 0.999849, 0.999765, 0.999661, 0.999539, 
  0.999398, 0.999238, 0.999059, 0.998862, 0.998645, 0.998410, 0.998156, 0.997884, 
  0.997592, 0.997282, 0.996953, 0.996606, 0.996240, 0.995855, 0.995451, 0.995029, 
  0.994588, 0.994129, 0.993651, 0.993154, 0.992639, 0.992105, 0.991553, 0.990982, 
  0.990393, 0.989785, 0.989159, 0.988514, 0.987851, 0.987170, 0.986470, 0.985752, 
  0.985016, 0.984261, 0.983488, 0.982697, 0.981888, 0.981061, 0.980215, 0.979352, 
  0.978470, 0.977571, 0.976653, 0.975718, 0.974764, 0.973793, 0.972804, 0.971797, 
  0.970772, 0.969730, 0.968670, 0.967592, 0.966497, 0.965384, 0.964253, 0.963105, 
  0.961940, 0.960757, 0.959557, 0.958340, 0.957105, 0.955853, 0.954584, 0.953298, 
  0.951995, 0.950675, 0.949337, 0.947983, 0.946612, 0.945225, 0.943820, 0.942399, 
  0.940961, 0.939506, 0.938035, 0.936548, 0.935044, 0.933523, 0.931987, 0.930434, 
  0.928864, 0.927279, 0.925678, 0.924060, 0.922427, 0.920778, 0.919113, 0.917432, 
  0.915735, 0.914023, 0.912295, 0.910551, 0.908793, 0.907018, 0.905229, 0.903424, 
  0.901604, 0.899769, 0.897919, 0.896053, 0.894174, 0.892279, 0.890369, 0.888444, 
  0.886505, 0.884552, 0.882584, 0.880601, 0.878605, 0.876594, 0.874568, 0.872529, 
  0.870476, 0.868409, 0.866327, 0.864232, 0.862124, 0.860002, 0.857866, 0.855716, 
  0.853554, 0.851378, 0.849188, 0.846986, 0.844771, 0.842542, 0.840301, 0.838047, 
  0.835780, 0.833500, 0.831208, 0.828904, 0.826587, 0.824258, 0.821916, 0.819563, 
  0.817197, 0.814819, 0.812430, 0.810029, 0.807616, 0.805192, 0.802756, 0.800309, 
  0.797850, 0.795380, 0.792899, 0.790407, 0.787904, 0.785391, 0.782866, 0.780331, 
  0.777785, 0.775229, 0.772663, 0.770086, 0.767499, 0.764902, 0.762295, 0.759678, 
  0.757052, 0.754416, 0.751770, 0.749114, 0.746450, 0.743775, 0.741092, 0.738400, 
  0.735699, 0.732989, 0.730270, 0.727542, 0.724806, 0.722061, 0.719308, 0.716547, 
  0.713778, 0.711000, 0.708215, 0.705422, 0.702621, 0.699812, 0.696996, 0.694173, 
  0.691342, 0.688504, 0.685659, 0.682807, 0.679948, 0.677082, 0.674210, 0.671331, 
  0.668445, 0.665554, 0.662656, 0.659751, 0.656841, 0.653925, 0.651003, 0.648076, 
  0.645143, 0.642204, 0.639260, 0.636311, 0.633357, 0.630397, 0.627433, 0.624464, 
  0.621491, 0.618512, 0.615530, 0.612543, 0.609551, 0.606556, 0.603556, 0.600553, 
  0.597546, 0.594535, 0.591520, 0.588503, 0.585481, 0.582457, 0.579430, 0.576399, 
  0.573366, 0.570330, 0.567291, 0.564249, 0.561206, 0.558160, 0.555111, 0.552061, 
  0.549009, 0.545955, 0.542899, 0.539842, 0.536783, 0.533723, 0.530661, 0.527598, 
  0.524534, 0.521470, 0.518404, 0.515338, 0.512271, 0.509204, 0.506136, 0.503068, 
  0.500000, 0.496933, 0.493865, 0.490797, 0.487730, 0.484663, 0.481597, 0.478531, 
  0.475467, 0.472403, 0.469340, 0.466279, 0.463218, 0.460159, 0.457102, 0.454046, 
  0.450992, 0.447940, 0.444889, 0.441841, 0.438795, 0.435751, 0.432710, 0.429671, 
  0.426635, 0.423602, 0.420571, 0.417544, 0.414520, 0.411498, 0.408480, 0.405466, 
  0.402455, 0.399448, 0.396445, 0.393445, 0.390450, 0.387459, 0.384472, 0.381489, 
  0.378510, 0.375537, 0.372568, 0.369604, 0.366644, 0.363690, 0.360741, 0.357797, 
  0.354858, 0.351925, 0.348998, 0.346076, 0.343160, 0.340249, 0.337345, 0.334447, 
  0.331555, 0.328670, 0.325791, 0.322919, 0.320053, 0.317194, 0.314342, 0.311497, 
  0.308659, 0.305828, 0.303005, 0.300188, 0.297380, 0.294579, 0.291786, 0.289000, 
  0.286223, 0.283454, 0.280692, 0.277939, 0.275195, 0.272459, 0.269731, 0.267012, 
  0.264302, 0.261601, 0.258909, 0.256225, 0.253551, 0.250887, 0.248231, 0.245585, 
  0.242949, 0.240323, 0.237706, 0.235099, 0.232502, 0.229915, 0.227338, 0.224771, 
  0.222215, 0.219670, 0.217135, 0.214610, 0.212096, 0.209593, 0.207101, 0.204621, 
  0.202151, 0.199692, 0.197245, 0.194809, 0.192385, 0.189972, 0.187571, 0.185181, 
  0.182804, 0.180438, 0.178085, 0.175743, 0.173414, 0.171097, 0.168793, 0.166501, 
  0.164221, 0.161954, 0.159700, 0.157459, 0.155230, 0.153015, 0.150812, 0.148623, 
  0.146447, 0.144284, 0.142135, 0.139999, 0.137877, 0.135768, 0.133673, 0.131592, 
  0.129525, 0.127471, 0.125432, 0.123407, 0.121396, 0.119399, 0.117417, 0.115449, 
  0.113495, 0.111556, 0.109632, 0.107722, 0.105827, 0.103947, 0.102082, 0.100232, 
  0.098397, 0.096577, 0.094772, 0.092982, 0.091208, 0.089449, 0.087706, 0.085978, 
  0.084265, 0.082569, 0.080888, 0.079223, 0.077574, 0.075940, 0.074323, 0.072721, 
  0.071136, 0.069567, 0.068014, 0.066477, 0.064957, 0.063453, 0.061965, 0.060494, 
  0.059040, 0.057602, 0.056180, 0.054776, 0.053388, 0.052017, 0.050663, 0.049326, 
  0.048006, 0.046702, 0.045416, 0.044147, 0.042895, 0.041661, 0.040443, 0.039243, 
  0.038061, 0.036895, 0.035747, 0.034617, 0.033504, 0.032408, 0.031331, 0.030271, 
  0.029228, 0.028203, 0.027197, 0.026207, 0.025236, 0.024283, 0.023347, 0.022430, 
  0.021530, 0.020648, 0.019785, 0.018939, 0.018112, 0.017303, 0.016512, 0.015739, 
  0.014985, 0.014248, 0.013530, 0.012830, 0.012149, 0.011486, 0.010841, 0.010215, 
  0.009607, 0.009018, 0.008447, 0.007895, 0.007361, 0.006846, 0.006349, 0.005871, 
  0.005412, 0.004971, 0.004549, 0.004145, 0.003760, 0.003394, 0.003047, 0.002718, 
  0.002408, 0.002116, 0.001844, 0.001590, 0.001355, 0.001139, 0.000941, 0.000762, 
  0.000602, 0.000461, 0.000339, 0.000235, 0.000151, 0.000085, 0.000038, 0.000009, 
};
#else
static float32_t hanning_window[FFT_SAMPLES];
#endif
SECTION_SRDSRAM float32_t fft_real_buffer[FFT_SAMPLES];		// FFT input buffer (only real part)
float32_t fft_result_buffer[FFT_SAMPLES];	// FFT result buffer (complex number)

float32_t float_mag_buffer[FFT_SAMPLES/2];

const float32_t Coeffs[NUMTAPS] = {
  0.00156601, -0.00182149,  0.00249659, -0.00359653,  0.00510147, -0.0069667,
  0.00912439, -0.01148685,  0.01395103, -0.01640417,  0.01873013, -0.02081595,
  0.02255847, -0.02387033,  0.02468516,  0.97349755,  0.02468516, -0.02387033,
  0.02255847, -0.02081595,  0.01873013, -0.01640417,  0.01395103, -0.01148685,
  0.00912439, -0.0069667,   0.00510147, -0.00359653,  0.00249659, -0.00182149,
  0.00156601
};

typedef struct {
  float32_t *putptr;
  float32_t *getptr;
  int16_t samples;      /* Accumurated data samples */
  int16_t magdiv;
} FFTINFO;

SECTION_SRDSRAM FFTINFO FftInfo;

typedef struct {
  FIL  *pfile;
  char *comments;
  char *fname;
  int  loop_count;
  int  loop_start;
  int  loop_end;
  int  pcm_pos;
  uint32_t seek_pos;
  osMessageQueueId_t *musicbufqId;
  osMessageQueueId_t *readqId;
} FLACINFO;

SECTION_SRDSRAM FLACINFO FlacInfo;

static SECTION_SRDSRAM arm_fir_decimate_instance_f32 decimate_instance;
static SECTION_SRDSRAM arm_rfft_fast_instance_f32 fft_instance;

typedef enum {
  MIX_DATA_REQ = 1,
  MIX_PLAY,
  MIX_PAUSE,
  MIX_RESUME,
  MIX_HALT,
  MIX_FFT_ENABLE,
  MIX_FFT_DISABLE,
  MIX_SET_VOLUME,
  /* Below events are for A2DP player */
  MIX_SET_POS,
  MIX_STREAM_START,
  MIX_STREAM_DATA,
  MIX_STREAM_STOP,
} mix_event;

typedef struct {
  mix_event event;
  void      *arg;
  int       option;
} MIXCONTROL_EVENT;

MIX_INFO MixInfo;

#define	MIX_EV_DEPTH	5

static osMutexId_t soundLockId;

static MIXCONTROL_EVENT mix_buffer[MIX_EV_DEPTH];

MESSAGEQ_DEF(mixevq, mix_buffer, sizeof(mix_buffer))

uint8_t FlacAllocSpace[DRLIB_HEAP_SIZE];

static uint8_t *flac_allocp;
static uint8_t *max_allocp;

static void *my_malloc(size_t sz, void *pUserData)
{
  UNUSED(pUserData);
  void *p;

  sz = (sz + 3) & ~3;		// Align on word boundary
// debug_printf("%s: %d, %x\n", __FUNCTION__, sz, flac_allocp);
  if (flac_allocp + sz > &FlacAllocSpace[DRLIB_HEAP_SIZE])
  {
    p = NULL;
    debug_printf("alloc failed.\n");
    osDelay(10*1000);
  }
  else
  {
    p = flac_allocp;
    flac_allocp += sz;
    if (flac_allocp > max_allocp)
    {
      max_allocp = flac_allocp;
      //debug_printf("drlib allocsize: %d\n", max_allocp - FlacAllocSpace);
    }
  }
  return p;
}

static void *my_realloc(void *p, size_t sz, void *pUserData)
{
  UNUSED(pUserData);
  void *vp;

  if (p)
  {
    vp = my_malloc(sz, NULL);
    if (vp)
      memcpy(vp, p, sz);
    return vp;
  }
// debug_printf("%s: %d\n", __FUNCTION__, sz);

  return NULL;
}

static void my_free(void *p, void *pUserData)
{
  UNUSED(p);
  UNUSED(pUserData);
  // debug_printf("my_free %x\n", p);
}

static size_t drflac__on_read_fatfs(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    UINT nread;
    FRESULT res;
    FLACINFO *piflac = (FLACINFO *)pUserData;

    res = f_read(piflac->pfile, pBufferOut, bytesToRead, &nread);
    if (res == FR_OK)
      return nread;
    return -1;
}

static drflac_bool32 drflac__on_seek_fatfs(void* pUserData, int offset, drflac_seek_origin origin)
{
    FRESULT res;
    FLACINFO *piflac = (FLACINFO *)pUserData;

    if (origin == drflac_seek_origin_current)
      res = f_lseek(piflac->pfile, f_tell(piflac->pfile) + offset);
    else
      res = f_lseek(piflac->pfile, offset);
    return (res == FR_OK);
}

/*===========================*/

static drflac_result drflac_fatopen(FIL** ppFile, const char* pFilePath)
{
    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || ppFile == NULL) {
        return DRFLAC_INVALID_ARGS;
    }

    *ppFile = OpenMusicFile((char *)pFilePath);
    if (*ppFile == NULL) {
        drflac_result result = DRFLAC_DOES_NOT_EXIST;
        if (result == DRFLAC_SUCCESS) {
            result = DRFLAC_ERROR;   /* Just a safety check to make sure we never ever return success when pFile == NULL. */
        }

        return result;
    }

    return DRFLAC_SUCCESS;
}

static drflac_vorbis_comment_iterator CommentIterator;

static void drflac__on_meta(void* pUserData, drflac_metadata* pMetadata)
{
    FLACINFO *piflac = (FLACINFO *)pUserData;

    if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
    {
      const char *cp;
      char *wp, *dp;
      drflac_uint32 clen;

      drflac_init_vorbis_comment_iterator(&CommentIterator, pMetadata->data.vorbis_comment.commentCount, pMetadata->data.vorbis_comment.pComments);
      dp = piflac->comments = (char *)my_malloc(pMetadata->rawDataSize, NULL);
      while (CommentIterator.countRemaining > 0)
      {
        cp = drflac_next_vorbis_comment(&CommentIterator, &clen);
        if (clen > 0)
        {
          memcpy(dp, cp, clen);
          wp = dp;
          dp += clen;
          *dp++ = 0;
          //debug_printf("%s\n", wp);
          if (strncmp(wp, "LOOP_START=", 11) == 0)
            piflac->loop_start = atoi(wp+11);
          else if (strncmp(wp, "LOOP_END=", 9) == 0)
            piflac->loop_end = atoi(wp+9);
        }
      }
    }
}

static drflac* drflac_open_fatfile(const char* pFileName, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    drflac* pFlac;
    FLACINFO *piflac;
    FIL* pFile;

    flac_allocp = &FlacAllocSpace[0];

    if (drflac_fatopen(&pFile, pFileName) != DRFLAC_SUCCESS) {
        return NULL;
    }
    piflac = (FLACINFO *)pAllocationCallbacks->pUserData;
    piflac->pfile = pFile;

    pFlac = drflac_open_with_metadata(drflac__on_read_fatfs, drflac__on_seek_fatfs,  drflac__on_meta, (void*)piflac, pAllocationCallbacks);
    if (pFlac == NULL) {
        CloseMusicFile(pFile);
        return NULL;
    }

    return pFlac;
}

static void drflac_close_fatfs(drflac* pFlac)
{
    if (pFlac == NULL) {
        return;
    }

    FLACINFO *piflac;
    piflac = (FLACINFO *)pFlac->bs.pUserData;

    /*
    If we opened the file with drflac_open_file() we will want to close the file handle. We can know whether or not drflac_open_file()
    was used by looking at the callbacks.
    */
    if (pFlac->bs.onRead == drflac__on_read_fatfs) {
        CloseMusicFile(piflac->pfile);
    }

#ifndef DR_FLAC_NO_OGG
    /* Need to clean up Ogg streams a bit differently due to the way the bit streaming is chained. */
    if (pFlac->container == drflac_container_ogg) {
        drflac_oggbs* oggbs = (drflac_oggbs*)pFlac->_oggbs;
        DRFLAC_ASSERT(pFlac->bs.onRead == drflac__on_read_ogg);

        if (oggbs->onRead == drflac__on_read_fatfs) {
            CloseMusicFile((FIL*)oggbs->pUserData);
        }
    }
#endif

    drflac__free_from_callbacks(pFlac, &pFlac->allocationCallbacks);
}

#if FFT_SAMPLES == 2048
static const int frange[] = { 8, 45, 300, 600, 0 };
#else
static const int frange[] = { 5, 23, 150, 300, 0 };
#endif
static int band_val[4];

int fft_getband(int band)
{
  return band_val[band];
}

int fft_getcolor(uint8_t *p)
{
  int v;

  v = band_val[0] + band_val[1];
  v <<= 1;
  *p++ = (v > 255)? 255 : v;
  v = band_val[2];
  v <<= 1;
  *p++ = (v > 255)? 255 : v;
  v = band_val[3];
  v <<= 1;
  *p = (v > 255)? 255 : v;
  v = band_val[0] + band_val[1] + band_val[2] + band_val[3];
  return v;
}

/*
 *  FFT process.
 *   Original sampling rate is 44.1k. We reduce the rate to 1/3,
 *   resulting as 14.7k.
 */
static int process_fft(FFTINFO *fftInfo, AUDIO_STEREO *pmusic, int frames)
{
  int i;

  HAL_GPIO_TogglePin(TEST0_GPIO_Port, TEST0_Pin);
  /* Convert to mono */
  for (i = 0; i < frames; i++)
  {
    sInBuffer[i] = (pmusic->ch0 + pmusic->ch1) / 2;
    pmusic++;
  }

  /* Convert int16_t values to float, then decimate */
  arm_q15_to_float(sInBuffer, sFloatBuffer, frames);
  arm_fir_decimate_f32(&decimate_instance, sFloatBuffer, fftInfo->putptr, frames);

  fftInfo->putptr += (frames/FFT_DECIMATION_FACTOR);
  if (fftInfo->putptr > &audio_buffer[AUDIO_SAMPLES])
  {
    debug_printf("audio_buffer overflow.\n");
    osDelay(100);
  }
  if (fftInfo->putptr == &audio_buffer[AUDIO_SAMPLES])
  {
    fftInfo->putptr = audio_buffer;
  }
  fftInfo->samples += frames/FFT_DECIMATION_FACTOR;

  if (fftInfo->samples >= FFT_SAMPLES)
  {
    int room;

//    debug_printf("putptr: %x, getptr: %x, samples = %d\n", fftInfo->putptr, fftInfo->getptr, fftInfo->samples);
    room = &audio_buffer[AUDIO_SAMPLES] - fftInfo->getptr;
    if (room >= FFT_SAMPLES)
    {
          arm_mult_f32(fftInfo->getptr, hanning_window, fft_real_buffer, FFT_SAMPLES);
    }
    else
    {
          arm_mult_f32(fftInfo->getptr, hanning_window, fft_real_buffer, room);
          arm_mult_f32(audio_buffer, hanning_window + room, fft_real_buffer + room, FFT_SAMPLES - room);
    }
    arm_rfft_fast_f32(&fft_instance, fft_real_buffer, fft_result_buffer, 0);
    arm_cmplx_mag_f32(fft_result_buffer, float_mag_buffer, FFT_SAMPLES/2);

    {
      int f, f_prev;
      const int *fp;
      int *op;
      float32_t v;

      f_prev = 0;
      fp = frange;
      op = band_val;
      while (*fp)
      {
        f = *fp++;
        v = 0.0;
        for (i = f_prev; i < f; i++)
        {
          v += float_mag_buffer[i];
        }
        if (v) v = v / fftInfo->magdiv;
        if (v < 0) v = 0;
        f_prev = f;
        *op++ = (int16_t)v;
      }
    //debug_printf("BAND: %d, %d, %d, %d\n", band_val[0], band_val[1], band_val[2], band_val[3]);
    }

    fftInfo->getptr += SHIFT_SAMPLES;
    if (fftInfo->getptr >= &audio_buffer[AUDIO_SAMPLES])
      fftInfo->getptr -= AUDIO_SAMPLES;
    fftInfo->samples -= SHIFT_SAMPLES; 
    HAL_GPIO_TogglePin(TEST0_GPIO_Port, TEST0_Pin);
    HAL_GPIO_TogglePin(TEST1_GPIO_Port, TEST1_Pin);
    return 1;	/* New FFT result is ready. */
  }
  HAL_GPIO_TogglePin(TEST0_GPIO_Port, TEST0_Pin);
  return 0;	/* No FFT result available. */
}

/**
 * @brief FLAC file reader task
 */
static void StartFlacReaderTask(void *args)
{
  UNUSED(args);
  FLACINFO *flacInfo = &FlacInfo;
  drflac_uint64 num_read;
  drflac_allocation_callbacks allocationCallbacks;
  drflac *pflac = NULL;
  AUDIO_STEREO *pmusic;
  osStatus_t st;
  uint16_t cmd;
  int i;
  int bindex;

  allocationCallbacks.pUserData = flacInfo;
  allocationCallbacks.onMalloc = my_malloc;
  allocationCallbacks.onRealloc = my_realloc;
  allocationCallbacks.onFree = my_free;

  while (1)
  {
    st = osMessageQueueGet(flacInfo->readqId, &cmd, 0, osWaitForever);

    if (st == osOK)
    {
      switch (cmd)
      {
      case READER_START:
        memset(MusicFrameBuffer, 0, sizeof(MusicFrameBuffer));
        flacInfo->loop_start = flacInfo->loop_end = flacInfo->pcm_pos = 0;
        pflac = drflac_open_fatfile(flacInfo->fname, &allocationCallbacks);
        if (pflac)
        {
          pmusic = MusicFrameBuffer;
          /* Read two frames of music data into MusicFrameBuffer. */
          num_read = drflac_read_pcm_frames_s16(pflac, NUM_FRAMES * BUF_FACTOR, (drflac_int16 *)pmusic);
          for (i = 0; i < BUF_FACTOR; i++)
          {
            osMessageQueuePut(flacInfo->musicbufqId, &pmusic, 0, 0);
            pmusic += NUM_FRAMES;
          }
          bindex = 0;
          flacInfo->pcm_pos = num_read;
        }
debug_printf("loop_count = %d\n", flacInfo->loop_count);
        break;
      case READER_READ:
        if (pflac)
        {
          pmusic = MusicFrameBuffer;
          if (bindex)
            pmusic += NUM_FRAMES;
          num_read = drflac_read_pcm_frames_s16(pflac, NUM_FRAMES, (drflac_int16 *)pmusic);
          if ((flacInfo->loop_count != 0) && (flacInfo->pcm_pos > flacInfo->loop_end))
          {
            /* We have passed loop_end positon, fix the num_read count. */
            num_read -= (flacInfo->pcm_pos - flacInfo->loop_end);
            flacInfo->seek_pos = flacInfo->loop_start;
debug_printf("need seek.\n");
          }
          if (num_read > 0)
          {
            AUDIO_STEREO *mp;

            flacInfo->pcm_pos += num_read;
            mp = pmusic + num_read;
            /* If read data amount is less than NUM_FRAMES,
             * fill with silent data.
             */
            while (num_read < NUM_FRAMES)
            {
              mp->ch0 = 0;
              mp->ch1 = 0;
              mp++;
              num_read++;
            }
            osMessageQueuePut(flacInfo->musicbufqId, &pmusic, 0, 0);
            if (flacInfo->seek_pos)
            {
debug_printf("PCM seek %d\n", flacInfo->seek_pos);
              drflac_seek_to_pcm_frame(pflac, flacInfo->loop_start);
              flacInfo->pcm_pos = flacInfo->loop_start;
              flacInfo->seek_pos = 0;
            }
            bindex ^= 1;
          }
          else
          {
            pmusic = NULL;
            osMessageQueuePut(flacInfo->musicbufqId, &pmusic, 0, 0);
            drflac_close_fatfs(pflac);
            pflac = NULL;
          }
        }
        break;
      case READER_STOP:
#ifdef MIX_DEBUG
        debug_printf("READER_STOP\n");
#endif
        if (pflac)
        {
          drflac_close_fatfs(pflac);
          pflac = NULL;
        }
        break;
      default:
        break;
      }
    }
  }
}

static void mix_half_comp()
{
  mix_request_data(0);
}

static void mix_full_comp()
{
  mix_request_data(1);
}

static void StartMixPlayerTask(void *args)
{
  AUDIO_OUTPUT_DRIVER *pDriver;
  FFTINFO *fftInfo;
  int fft_count;
  MIX_INFO *mixInfo = &MixInfo;
  FLACINFO *flacInfo = &FlacInfo;
  GUI_EVENT guiev;
  int argval;
  uint8_t stream_toggle = 0;
  uint32_t psec;
  AUDIO_CONF *audio_config;
  HAL_DEVICE *haldev = &HalDevice;
  uint16_t cmd;
  int mix_frames;
  AUDIO_INIT_PARAMS *param = (AUDIO_INIT_PARAMS *)args;

  debug_printf("Player Started..\n");

  audio_config = get_audio_config(NULL);

  argval = audio_config->devconf->mix_mode;

  flacInfo->readqId = osMessageQueueNew(MIXREADQ_DEPTH, sizeof(uint16_t), &attributes_flacreadq);
  flacInfo->musicbufqId = osMessageQueueNew(BUF_FACTOR, sizeof(uint32_t *), &attributes_musicbufq);
  mixInfo->musicbufqId = flacInfo->musicbufqId;

  if (haldev->boot_mode == BOOTM_DOOM)
  {
    osThreadNew(StartFlacReaderTask, NULL, &attributes_flacreader);
  }

  fftInfo = &FftInfo;
  fftInfo->magdiv = param->fft_magdiv;
  fft_count = 0;
  arm_fir_decimate_init_f32(&decimate_instance, NUMTAPS, FFT_DECIMATION_FACTOR, (float32_t *)Coeffs, DeciStateBuffer, NUM_FRAMES);
#if FFT_SAMPLES != 1024
  arm_hanning_f32(hanning_window, FFT_SAMPLES);
#endif

  pDriver = (AUDIO_OUTPUT_DRIVER *)audio_config->devconf->pDriver;
  mix_frames = NUM_FRAMES;

  mixInfo->mixevqId = osMessageQueueNew(MIX_EV_DEPTH, sizeof(MIXCONTROL_EVENT), &attributes_mixevq);
  mixInfo->volume = AUDIO_DEF_VOL;
  mixInfo->state = MIX_ST_IDLE;

  if (haldev->boot_mode != BOOTM_OSCM)
  {
    mixInfo->volume = param->volume;
    pDriver->Init(audio_config, param);
  }
  else
  {
    pDriver->SetVolume(audio_config, mixInfo->volume);
  }
  soundLockId = audio_config->soundLockId;

  /* We'll keep sending contents of FinalSoundBuffer using DMA */

  if (haldev->boot_mode == BOOTM_DOOM)
  {
    pDriver->Start(audio_config);
  }

  while (1)
  {
    MIXCONTROL_EVENT ctrl;
    AUDIO_STEREO *mp;
    osStatus_t st;

    osMessageQueueGet(mixInfo->mixevqId, &ctrl, 0, osWaitForever);

    switch (ctrl.event)
    {
    case MIX_PLAY:		// Start playing specified FLAC file
#ifdef MIX_DEBUG
      debug_printf("MIX_PLAY\n");
#endif
      if (mixInfo->state != MIX_ST_IDLE)
      {
        debug_printf("MIX_PLAY: Bad state\n");
      }

      if (haldev->boot_mode == BOOTM_DOOM)
      {
        flacInfo->fname = (char *)ctrl.arg;
        flacInfo->loop_count = ctrl.option;
        if (flacInfo->loop_count > 0)
          flacInfo->loop_count--;
      }

      mixInfo->ppos = mixInfo->psec = 0;
      fft_count = 0;

      memset(audio_buffer, 0, sizeof(audio_buffer));
      fftInfo->getptr = fftInfo->putptr = audio_buffer;
      fftInfo->samples = 0;
      arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);

      if (haldev->boot_mode == BOOTM_DOOM)
      {
        cmd = READER_START;
        if (osMessageQueuePut(flacInfo->readqId, &cmd, 0, 0) != osOK)
        {
          debug_printf("readq full\n");
        }
      }
      mixInfo->state = MIX_ST_PLAY;
      break;
    case MIX_DATA_REQ:
      /* Swap freebuffer_ptr value. */
      if (ctrl.option)
        audio_config->freebuffer_ptr = audio_config->sound_buffer + NUM_FRAMES;
      else
        audio_config->freebuffer_ptr = audio_config->sound_buffer;

      switch (mixInfo->state)
      {
      case MIX_ST_PLAY:
        /* Try to get FLAC decoded buffer */
        st = osMessageQueueGet(flacInfo->musicbufqId, &mp, 0, 0);
        if (st == osOK)
        {
          if (mp)
          {
            if (haldev->boot_mode == BOOTM_DOOM)
            {
              /* Got decoded buffer. Request to start decoding next block of frames. */
              cmd = READER_READ;
              if (osMessageQueuePut(flacInfo->readqId, &cmd, 0, 0) != osOK)
              {
                debug_printf("failed to request read.\n");
              }
            }

            if (argval & MIXER_FFT_ENABLE)
            {
              if (process_fft(fftInfo, mp, NUM_FRAMES))
              {
                guiev.evcode = GUIEV_FFT_UPDATE;
                guiev.evval0 = fft_count;
                guiev.evarg1 = band_val;
                postGuiEvent(&guiev);
                fft_count++;
              }
            }
            pDriver->MixSound(audio_config, mp, mix_frames);
            mixInfo->ppos += NUM_FRAMES;
          }
          else
          {
            /* Reached to EOF */
            GUI_EVENT guiev;
  
            mixInfo->state = MIX_ST_IDLE;
            mixInfo->idle_count = IDLE_PERIOD;

debug_printf("Music Finish.\n");
            if (argval & MIXER_FFT_ENABLE)
            {
              guiev.evcode = GUIEV_MUSIC_FINISH;
              postGuiEvent(&guiev);
            }
          }
        }
        else
        {
          mp = NULL;
        }
        if (mp == NULL)
        {
          /* We have no decoded music buffer. Send silent sound. */
          pDriver->MixSound(audio_config, SilentBuffer, mix_frames);
        }
        break;
      case MIX_ST_IDLE:
        if (fftInfo->samples > 0)
        {
          if (argval & MIXER_FFT_ENABLE)
          {
            if (process_fft(fftInfo, (AUDIO_STEREO *)SilentBuffer, NUM_FRAMES))
            {
              guiev.evcode = GUIEV_FFT_UPDATE;
              guiev.evval0 = fft_count;
              guiev.evarg1 = band_val;
              postGuiEvent(&guiev);
              fft_count++;
            }
          }
          if (mixInfo->idle_count > 0)
          {
             mixInfo->idle_count--;
             if (mixInfo->idle_count == 0)
               fftInfo->samples = 0;
          }
        }
        pDriver->MixSound(audio_config, SilentBuffer, mix_frames);
        break;
      default:
        debug_printf("st = %d\n", mixInfo->state);
        break;
      }
      psec = mixInfo->ppos / audio_config->devconf->playRate;
      if (psec > mixInfo->psec)
      {
        mixInfo->psec = psec;
        if (argval & MIXER_FFT_ENABLE)
        {
          postGuiEventMessage(GUIEV_PSEC_UPDATE, psec, mixInfo, NULL);
        }
      }
      break;
    case MIX_PAUSE:
#ifdef MIX_DEBUG
      debug_printf("MIX_PAUSE\n");
#endif
      mixInfo->state = MIX_ST_IDLE;
      mixInfo->idle_count = IDLE_PERIOD;
      break;
    case MIX_RESUME:
#ifdef MIX_DEBUG
      debug_printf("MIX_RESUME\n");
#endif
      mixInfo->state = MIX_ST_PLAY;
      break;
    case MIX_FFT_DISABLE:
      debug_printf("FFT_DISABLE\n");
      argval &= ~MIXER_FFT_ENABLE;
      memset(band_val, 0, sizeof(band_val));
      break;
    case MIX_FFT_ENABLE:
      argval |= MIXER_FFT_ENABLE;
      break;
    case MIX_HALT:
#ifdef MIX_DEBUG
      debug_printf("MIX_HALT\n");
#endif
      if (haldev->boot_mode == BOOTM_DOOM)
      {
        cmd = READER_STOP;
        if (osMessageQueuePut(flacInfo->readqId, &cmd, 0, 0) != osOK)
        {
          debug_printf("readq full\n");
        }
      }
      mixInfo->state = MIX_ST_IDLE;
      mixInfo->idle_count = IDLE_PERIOD;
      break;
    case MIX_SET_VOLUME:
#ifdef MIX_DEBUG
      debug_printf("SetVolume: %d\n", ctrl.arg);
#endif
      mixInfo->volume = (int) ctrl.arg;
      pDriver->SetVolume(audio_config, mixInfo->volume);
      break;
    case MIX_SET_POS:
      mixInfo->song_len = (uint32_t)ctrl.arg * 441 / 10;	// Convert to samples
      mixInfo->ppos = ctrl.option * 441 / 10;		// Convert to samples
      mixInfo->psec = mixInfo->ppos / 44100;		// Convert to secs
      postGuiEventMessage(GUIEV_PSEC_UPDATE, mixInfo->psec, mixInfo, NULL);
      break;
    case MIX_STREAM_START:
      fft_count = 0;
      fftInfo->getptr = fftInfo->putptr = audio_buffer;
      fftInfo->samples = 0;
      arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);
      break;
    case MIX_STREAM_DATA:
      mp = (AUDIO_STEREO *)ctrl.arg;
      if (ctrl.option != NUM_FRAMES)
        debug_printf("%d frames.\n", ctrl.option);
      if (stream_toggle && (process_fft(fftInfo, mp, ctrl.option)))
      {
        guiev.evcode = GUIEV_FFT_UPDATE;
        guiev.evval0 = fft_count;
        guiev.evarg1 = band_val;
        postGuiEvent(&guiev);
        fft_count++;
      }
      stream_toggle ^= 1;
      mixInfo->ppos += ctrl.option;
      psec = mixInfo->ppos / 44100;
      if (psec != mixInfo->psec)
      {
        mixInfo->psec = psec;
        postGuiEventMessage(GUIEV_PSEC_UPDATE, psec, mixInfo, NULL);
      }
      break;
    case MIX_STREAM_STOP:
      break;
    default:
      debug_printf("event = %x\n", ctrl.event);
      break;
    }
  }
  pDriver->Stop(audio_config);
}

int Mix_GetVolume()
{
  return MixInfo.volume;
}

/*
 * Public functions to suppoort SDL_Mixer API.
 */

static MUSIC_STATE play_state;

void Mix_FFT_Enable()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_FFT_ENABLE;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
}

void Mix_FFT_Disable()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_FFT_DISABLE;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
}

int Mix_PlayMusic(Mix_Music *music, int loop)
{
  MIXCONTROL_EVENT mixc;
  int st;

  mixc.event = MIX_PLAY;
  mixc.arg = (void *)music->fname;
  mixc.option = loop;
  play_state = music->state = MST_PLAYING;
  if (music->magic != MIX_MAGIC)
     debug_printf("Bad magic.\n");
  st = osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
  if (st != osOK)
    debug_printf("failed to put. %d\n", st);
  return 0;
}

int Mix_ResumeMusic(Mix_Music *music)
{
  MIXCONTROL_EVENT mixc;

  switch (music->state)
  {
  case MST_LOADED:
    Mix_PlayMusic(music, 0);
    break;
  case MST_PAUSED:
    mixc.event = MIX_RESUME;
    play_state = music->state = MST_PLAYING;
    if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
    {
      debug_printf("%s: put failed.\n", __FUNCTION__);
    }
    break;
  default:
    break;
  }
  return 0;
}

int Mix_PauseMusic(Mix_Music *music)
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_PAUSE;
  play_state = music->state = MST_PAUSED;
  if (music->magic != MIX_MAGIC)
     debug_printf("Bad magic.\n");
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
  return 0;
}

void Mix_HaltMusic()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_HALT;
  mixc.arg = NULL;
  play_state = MST_INIT;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
}

Mix_Music *Mix_LoadMUS(const char *file)
{
  Mix_Music *music;

  music = (Mix_Music *)malloc(sizeof(Mix_Music));
  if (music)
  {
    music->magic = MIX_MAGIC;
    music->fname = file;
    play_state = music->state = MST_LOADED;
  }
  else
  {
    debug_printf("%s: no mem.\n", __FUNCTION__);
    while (1) osDelay(200);
  }
  return music;
}

int Mix_PlayingMusic()
{
  return (play_state == MST_PLAYING)? 1 : 0;
}

int Mix_VolumeMusic(int volume)
{
#if 0
  MIXCONTROL_EVENT mixc;
#endif

  if (volume < 0)
    volume = 75;

#if 0
  mixc.event = MIX_SET_VOLUME;
  mixc.arg = (void *)volume;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
#endif

  return volume;
}

void Mix_FreeMusic(Mix_Music *music)
{
  if (music )
  {
     if (music->magic != MIX_MAGIC)
       debug_printf("BAD MAGIC %x\n", music->magic);
     music->magic = 0;
     free(music);
  }
}

static osThreadId_t mixid;

void Start_SDLMixer()
{
  if (mixid == 0)
  {
    mixid = osThreadNew(StartMixPlayerTask, (void *)&doom_audio_params, &attributes_mixplayer);
  }
}

void Start_A2DPMixer(AUDIO_INIT_PARAMS *params)
{
    osThreadNew(StartMixPlayerTask, (void *)params, &attributes_mixplayer);
}

void Start_Doom_SDLMixer()
{
  if (mixid == 0)
  {
    mixid = osThreadNew(StartMixPlayerTask, (void *)&doom_audio_params, &attributes_mixplayer);
  }
}

int Mix_Started()
{
  return (int)mixid;
}

void Stop_SDLMixer()
{
  osThreadTerminate(mixid);
  mixid = 0;
}

CHANINFO ChanInfo[NUM_CHANNELS];

#define	MIX_DEBUGx

void LockChanInfo()
{
  osMutexAcquire(soundLockId, osWaitForever);
}

void UnlockChanInfo()
{
  osMutexRelease(soundLockId);
}

/**
 * @brief Start playing given sound chunk. This function is called by DOOM i_sdlsound.c
 */
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d, %d\n", __FUNCTION__, channel, chunk->alen);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].chunk = chunk;
  ChanInfo[channel].loop = loops;
  ChanInfo[channel].pread = (AUDIO_STEREO *)chunk->abuf;
  ChanInfo[channel].plast = (AUDIO_STEREO *)(chunk->abuf + chunk->alen);
  ChanInfo[channel].flag |= FL_SET|FL_PLAY;
  osMutexRelease(soundLockId);
  return channel;
}

/**
 * @brief Return current sound playing sample poistion in pusedoRate.
 */
int Mix_PlayPosition(int channel)
{
  Mix_Chunk *chunk;
  int pos;

  osMutexAcquire(soundLockId, osWaitForever);
  chunk = ChanInfo[channel].chunk;
  pos = ChanInfo[channel].pread - (AUDIO_STEREO *)chunk->abuf;
  osMutexRelease(soundLockId);

  return pos;
}

int Mix_LoadChannel(int channel, Mix_Chunk *chunk, int loops)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d, %d\n", __FUNCTION__, channel, chunk->alen);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].chunk = chunk;
  ChanInfo[channel].loop = loops;
  ChanInfo[channel].flag = FL_SET;
  ChanInfo[channel].pread = (AUDIO_STEREO *)chunk->abuf;
  ChanInfo[channel].plast = (AUDIO_STEREO *)(chunk->abuf + chunk->alen);
  ChanInfo[channel].vol_left = MIX_MAX_VOLUME;
  ChanInfo[channel].vol_right = MIX_MAX_VOLUME;
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_ResumeChannel(int channel)
{
  osMutexAcquire(soundLockId, osWaitForever);
#if 1
  ChanInfo[channel].flag |= FL_SET|FL_PLAY;
#else
  ChanInfo[channel].flag |= FL_PLAY;
#endif
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_PauseChannel(int channel)
{
  osMutexAcquire(soundLockId, osWaitForever);
#if 0
  ChanInfo[channel].flag &= ~FL_SET;
#else
  ChanInfo[channel].flag &= ~FL_PLAY;
#endif
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_HaltChannel(int channel)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d\n", __FUNCTION__, channel);
#endif

  osMutexAcquire(soundLockId, osWaitForever);
  if (channel >= 0)
  {
    ChanInfo[channel].flag &= ~(FL_PLAY|FL_SET);
  }
  else	/* Close all channels */
  {
    for (channel = 0; channel < NUM_CHANNELS; channel++)
      ChanInfo[channel].flag &= ~(FL_PLAY|FL_SET);
  }
  osMutexRelease(soundLockId);
  return 0;
}

/*
 * @brief See if given sound chanel is busy. Called by i_sdlsound.c
 */
int Mix_Playing(int channel)
{
  //debug_printf("%s %d:\n", __FUNCTION__, channel);
  if ((ChanInfo[channel].flag & (FL_SET|FL_PLAY)) == (FL_SET|FL_PLAY))
    return 1;
  return 0;
}

int Mix_SetPanning(int channel, uint8_t left, uint8_t right)
{
  CHANINFO *cinfo = &ChanInfo[channel];
  int res = 0;

#ifdef MIX_DEBUG
  debug_printf("%s: %d (%d, %d)\n", __FUNCTION__, channel, left, right);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  if (cinfo->pread < cinfo->plast)
  {
    cinfo->vol_left = left;
    cinfo->vol_right = right;
    //cinfo->flag |= FL_PLAY;
    res = 1;
  }
  osMutexRelease(soundLockId);
  return res;
}

void Mix_CloseAudio()
{
  debug_printf("%ss:\n", __FUNCTION__);
}

int Mix_AllocateChannels(int chans)
{
  int i;

  osMutexAcquire(soundLockId, osWaitForever);
  for (i = 0; i < chans; i++)
    ChanInfo[i].flag = FL_ALLOCED;
  osMutexRelease(soundLockId);
  return 1;
}

static void mix_request_data(int full)
{
  MIXCONTROL_EVENT evcode;
  int st;

  evcode.event = MIX_DATA_REQ;
  evcode.option = full;
  st = osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
  if (st != osOK)
    debug_printf("mix request failed (%d)\n", st);
}

int Mix_QueryFreq()
{
  AUDIO_CONF *aconf;

  aconf = get_audio_config(NULL);
  return aconf->devconf->playRate;
}

void Mix_Set_Position(int length, int pos)
{
  MIXCONTROL_EVENT evcode;

  evcode.event = MIX_SET_POS;
  evcode.arg = (void *)length;
  evcode.option = pos;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

static AUDIO_STEREO *audio_bufp;

void Mix_Stream_Data(uint8_t *ptr, int num_frames)
{
  MIXCONTROL_EVENT evcode;

  memcpy(audio_bufp, ptr, num_frames * sizeof(AUDIO_STEREO));
  evcode.event = MIX_STREAM_DATA;
  evcode.arg = (void *)audio_bufp;
  audio_bufp += num_frames;
  if (audio_bufp >= &MusicFrameBuffer[BUF_FRAMES])
    audio_bufp = MusicFrameBuffer;

  evcode.option = num_frames;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

void Mix_Stream_Start()
{
  MIXCONTROL_EVENT evcode;

  audio_bufp = MusicFrameBuffer;
  evcode.event = MIX_STREAM_START;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

void Mix_Stream_Stop()
{
  MIXCONTROL_EVENT evcode;

  evcode.event = MIX_STREAM_STOP;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}
