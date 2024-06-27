#include "DoomPlayer.h"
#include "doom/d_player.h"
#include "doom/doomstat.h"
#include "app_gui.h"
#include "doomkeys.h"

#define	KBDQ_DEPTH	20

KBDEVENT kbdqBuffer[KBDQ_DEPTH];

MESSAGEQ_DEF(kbdq, kbdqBuffer, sizeof(kbdqBuffer))

static osMessageQueueId_t kbdqId;

extern void send_padkey(lv_indev_data_t *pdata);

/**
 * @brief Returns RGB color based on current health level.
 * @arg cval pointer to RGB array.
 */
void GetPlayerHealthColor(uint8_t *cval)
{
  int hval;
#ifdef HEALTH_DEBUG
  static int hcount;
#endif

  if (players[consoleplayer].mo == NULL)
  {
    cval[0] = cval[1] = cval[2] = 0;
    return;
  }

  hval = players[consoleplayer].mo->health;
  if (hval > 300)
    hval = 300;
  if (hval < 0)
    hval = 0;
#ifdef HEALTH_DEBUG
  if (++hcount % 50 == 0)
  {
   debug_printf("hval (%d) = %d\n", consoleplayer, hval);
  }
#endif

  cval[0] = cval[1] = cval[2] = 0;

  if (hval >= 80)		// Completey healthy
  {
    cval[1] = 255;		// Green
  }
  else if (hval >= 60)
  {
    cval[0] = 128;
    cval[1] = 255;
  }
  else if (hval >= 40)
  {
    cval[0] = 255;
    cval[1] = 255;
  }
  else if (hval >= 20)
  {
    cval[0] = 255;
    cval[1] = 130;
  }
  else if (hval > 0)
  {
    cval[0] = 255;
  }
}

/*
 * @brief Send cheat text character to DoomTask.
 */
void doom_send_cheat_key(char ch)
{
  static KBDEVENT ckevent;

  ckevent.evcode = KBDEVENT_DOWN;
  ckevent.asciicode = ckevent.doomcode = ch;

  if (kbdqId == 0)
  {
    kbdqId = osMessageQueueNew(KBDQ_DEPTH, sizeof(KBDEVENT), &attributes_kbdq);
  }
  if (kbdqId)
  {
    if (osMessageQueuePut(kbdqId, &ckevent, 0, 0) != osOK)
    {
      debug_printf("%s: put failed.\n", __FUNCTION__);
    }
  }
}

KBDEVENT *kbd_get_event()
{
  static KBDEVENT kevent;
  osStatus_t st;

  if (kbdqId)
  {
    st = osMessageQueueGet(kbdqId, &kevent, NULL, 0);
    if (st == osOK)
    {
      return &kevent;
    }
  }
  return NULL;
}
