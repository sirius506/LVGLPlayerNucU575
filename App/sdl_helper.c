#include "DoomPlayer.h"
#include "SDL.h"
#include "app_gui.h"
#include "m_misc.h"

int SDL_Init(uint32_t flags)
{
  UNUSED(flags);
  return 0;
}

int SDL_Quit()
{
  debug_printf("%s called.!\n", __FUNCTION__);
  postMainRequest(REQ_END_DOOM, NULL, 0);
  while (1)
   osDelay(100);
}

uint32_t SDL_GetTicks(void)
{
  return osKernelGetTickCount();
}

void SDL_Delay(uint32_t ms)
{
  osDelay(ms);
}

int SDL_ShowSimpleMessageBox(uint32_t flags, const char *title, const char *message, void *window)
{
  UNUSED(flags);
  UNUSED(window);

  debug_printf("%s: %s.!\n", title, message);
  postGuiEventMessage(GUIEV_ERROR_MESSAGE, 0, (void *)message, NULL);
  osDelay(5000);
  return 0;
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
  UNUSED(org);
  UNUSED(app);

  return M_StringDuplicate("/");
}
