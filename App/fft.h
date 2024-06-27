#define FFT_DECIMATION_FACTOR   3

#define AUDIO_SAMPLES   (NUM_FRAMES/FFT_DECIMATION_FACTOR*20)	// 3200

#if 0
#define FFT_SAMPLES     2048
#define SHIFT_SAMPLES   (FFT_SAMPLES/4)
#define MAGDIV          10
#else
#define FFT_SAMPLES     1024
#define SHIFT_SAMPLES   (FFT_SAMPLES/2)
#define MAGDIV          3
#endif
