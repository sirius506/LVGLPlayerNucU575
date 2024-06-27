#include "DoomPlayer.h"
#include "app_music.h"

static COVER_INFO *cover_top;

COVER_INFO *track_cover(int track)
{
  COVER_INFO *cfp = cover_top;

  if (cfp == NULL)
    return NULL;

  if (track < 0) track = 0;
  while (track > 0)
  {
    track--;
    cfp = cfp->next;
    if (cfp == NULL)
      cfp = cover_top;
  }
  return cfp;
}

int register_cover_file()
{
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_FLASH_ADDR;
  FS_DIRENT *dirent;
  COVER_INFO *cfp;
  int i;
  uint32_t *bp;
  char mbuffer[20];

  dirent = dirInfo->fs_direntry;
  cover_top = NULL;

  for (i = 0; i < NUM_DIRENT-1; i++)
  {
    if (dirent->foffset == 0xFFFFFFFF)
      break; 
    memcpy(mbuffer, dirent->fname, FS_NAMELEN);
    if (strncasecmp(mbuffer + strlen(mbuffer) - 4, ".bin", 4) == 0)
    {
      bp = (uint32_t *)(QSPI_FLASH_ADDR + dirent->foffset);
      if (*bp == COVER_MAGIC)
      {
        cfp = (COVER_INFO *)lv_malloc(sizeof(COVER_INFO));
        cfp->fname = dirent->fname;
        cfp->faddr = (uint8_t *) bp;
        cfp->fsize = dirent->fsize;
        cfp->next = cover_top;
        cover_top = cfp;
      }
    }
    dirent++;
  }
  return i;
}
