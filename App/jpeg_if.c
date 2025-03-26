#include "DoomPlayer.h"
#include "lvgl.h"
#include "jpeg_if.h"

EVFLAG_DEF(jpegflag)

#define	N_BPP	(3 - JD_FORMAT)

extern const osThreadAttr_t attributes_flacreader;

void StartJpegTask(void *arg);

typedef struct {
  uint8_t      *buffer;
  int16_t      blength;
  int16_t      read_index;
  int16_t      jwidth;
  uint16_t     gotEOF;
  uint8_t      *fb;		/* Frame buffer */
  osEventFlagsId_t flagsId;
} JPEGIF_INFO;

JPEGIF_INFO JpegIfInfo;

static lv_image_dsc_t imgdesc;

static size_t tjpgd_infunc(JDEC *jd, uint8_t *buff, size_t nbyte)
{
    JPEGIF_INFO *jpegInfo = (JPEGIF_INFO *)jd->device;
    uint32_t flag;
    size_t nb = 0;
    size_t tread = 0;
    size_t remain = nbyte;

#ifdef JPEG_DEBUG
debug_printf("infunc: nb = %d (blen = %d, rindex = %d)\n", nbyte, jpegInfo->blength, jpegInfo->read_index);
#endif
    if (buff)
    {
      while (tread < nbyte)
      {
        nb = jpegInfo->blength - jpegInfo->read_index;
        if (nb <= 0)
        {
          flag = osEventFlagsWait(jpegInfo->flagsId, (JPEGEV_FEED|JPEGEV_EOF), osFlagsWaitAny, osWaitForever);
          if (flag & JPEGEV_EOF)
          {
            jpegInfo->gotEOF = 1;
debug_printf("Got EOF\n");
            return tread;
          }
#ifdef JPEG_DEBUG
debug_printf("New FEED\n");
#endif
          jpegInfo->read_index = 0;
          nb = jpegInfo->blength;
        }
        if (nb > 0)
        {
          if (remain < nb)
            nb = (int)remain;
          memcpy(buff, jpegInfo->buffer + jpegInfo->read_index, nb);
#ifdef JPEG_DEBUG
debug_printf("Copy: %d (%d)\n", nb, jpegInfo->read_index);
#endif
          jpegInfo->read_index += nb;
          tread += nb;
          remain -= nb;
          if (jpegInfo->read_index >= jpegInfo->blength)
          {
#ifdef JPEG_DEBUG
            debug_printf("Set EMPTY: blen = %d, index = %d\n", jpegInfo->blength, jpegInfo->read_index);
#endif
            osEventFlagsSet(jpegInfo->flagsId, JPEGEV_EMPTY);
            buff += nb;
          }
        }
      }
      return tread;
    }
    else
    {
       /* Need to skip nbytes input */
      size_t sb;

      nb = 0;
      while (nbyte > 0)
      {
        sb = jpegInfo->blength - jpegInfo->read_index;
        if (sb < nbyte)
        {
          nbyte -= sb;
          nb += sb;
          osEventFlagsSet(jpegInfo->flagsId, JPEGEV_EMPTY);
          flag = osEventFlagsWait(jpegInfo->flagsId, (JPEGEV_FEED|JPEGEV_EOF), osFlagsWaitAny, osWaitForever);
          jpegInfo->read_index = 0;
          if (flag & JPEGEV_EOF)
            jpegInfo->gotEOF = 1;
        }
        else
        {
          sb = nbyte;
          jpegInfo->read_index += sb;
          nbyte = 0;
          nb += sb;
        }
      }
    }
    return nb;
}

static int tjpgd_outfunc(JDEC *jd, void *bitmap, JRECT *rect)
{
    JPEGIF_INFO *jpegInfo = (JPEGIF_INFO *)jd->device;
    uint8_t *src, *dst;
    uint16_t y, bws;
    uint32_t bwd;

    /* Copy the output image rectangle to the frame buffer */
    src = (uint8_t *)bitmap;
    dst = jpegInfo->fb + N_BPP * (rect->top * jpegInfo->jwidth + rect->left);
    bws = N_BPP * (rect->right - rect->left + 1);
    bwd = N_BPP * jpegInfo->jwidth;
    for (y = rect->top; y <= rect->bottom; y++)
    {
      memcpy(dst, src, bws);	/* Copy a line */
      src += bws; dst += bwd;
    }
    return 1;
}

void jpegif_init()
{
  lv_image_header_t *header;
  JPEGIF_INFO *jpegInfo = &JpegIfInfo;

  header = &imgdesc.header;
  header->magic = LV_IMAGE_HEADER_MAGIC;
#if JD_FORMAT == 0
  header->cf = LV_COLOR_FORMAT_RGB888;
#else
  header->cf = LV_COLOR_FORMAT_RGB565;
#endif
  header->w = header->h = 200;
  header->stride = 200 * N_BPP;
  imgdesc.data_size = header->w * header->h * N_BPP;
  imgdesc.data = malloc(200*200*N_BPP);

  jpegInfo->fb = (uint8_t *)imgdesc.data;
  jpegInfo->flagsId = osEventFlagsNew(&attributes_jpegflag);

  osThreadNew(StartJpegTask, NULL, &attributes_flacreader);
}

void jpegif_start()
{
  JPEGIF_INFO *jpegInfo = &JpegIfInfo;

  osEventFlagsSet(jpegInfo->flagsId, JPEGEV_START);
}

void jpegif_stop()
{
  JPEGIF_INFO *jpegInfo = &JpegIfInfo;

debug_printf("jpegif_stop called.\n");
  osEventFlagsSet(jpegInfo->flagsId, JPEGEV_EOF);
}

void jpegif_write(uint8_t *packet, int len)
{
  JPEGIF_INFO *jpegInfo = &JpegIfInfo;

#ifdef JPEG_DEBUG
debug_printf("jpegif_write: %d\n", len);
#endif
  jpegInfo->buffer = packet;
  jpegInfo->blength = len;
  osEventFlagsSet(jpegInfo->flagsId, JPEGEV_FEED);
  osEventFlagsWait(jpegInfo->flagsId, JPEGEV_EMPTY, osFlagsWaitAny, osWaitForever);
#ifdef JPEG_DEBUG
debug_printf("jpegif_write: done\n");
#endif
}

#define	TJPG_WBSIZE	3500
static uint8_t tjpg_work_buffer[TJPG_WBSIZE];

void StartJpegTask(void *arg)
{
  UNUSED(arg);
  JRESULT res;
  JDEC jdec;
  JPEGIF_INFO *jpegInfo = &JpegIfInfo;

  while (1)
  {
#ifdef JPEG_DEBUG
debug_printf("Waiting for START\n");
#endif
    osEventFlagsWait(jpegInfo->flagsId, JPEGEV_START, osFlagsWaitAny, osWaitForever);
debug_printf("Got START\n");
    jpegInfo->gotEOF = 0;
    res = jd_prepare(&jdec, tjpgd_infunc, tjpg_work_buffer, TJPG_WBSIZE, jpegInfo);
    if (res == JDR_OK)
    {
#ifdef JPEG_DEBUG
debug_printf("JPEG: %d x %d\n", jdec.width, jdec.height);
#endif
      if ((jdec.width = 200) && (jdec.height == 200))
      {
        jpegInfo->jwidth = jdec.width;

        res = jd_decomp(&jdec, tjpgd_outfunc, 0);
        if (res == JDR_OK)
        {
          debug_printf("Cover Valid\n");
          postGuiEventMessage(GUIEV_COVER_ART, 200*2*200, &imgdesc, &imgdesc);
        }
        else
        {
          debug_printf("decomp failed (%d)\n", res);
        }
      }
    }
    else
    {
debug_printf("prepare failed %d\n", res);
    }
    osEventFlagsClear(jpegInfo->flagsId, JPEGEV_EMPTY|JPEGEV_FEED|JPEGEV_EOF);
  }
}
