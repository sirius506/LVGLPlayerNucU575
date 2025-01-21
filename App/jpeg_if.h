#ifndef JPEG_IF_H
#define	JPEG_IF_H
#include "tjpgd.h"

typedef enum {
  JPEGEV_START = 1,
  JPEGEV_FEED = 2,
  JPEGEV_EOF = 4,
  JPEGEV_EMPTY = 8
} JPEG_EVENT;

void jpegif_init();
void jpegif_start();
void jpegif_stop();
void jpegif_write(uint8_t *packet, int len);
#endif
