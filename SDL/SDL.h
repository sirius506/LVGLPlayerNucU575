#ifndef __SDL_H__
#define __SDL_H__
#include <stdint.h>
#include <string.h>

#define	SDL_MESSAGEBOX_ERROR	0
#define	SDL_CONTROLLER_BUTTON_MAX 16

#define	SDL_free	free

#define	SDL_INIT_TIMER	0

typedef int SDL_bool;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

extern int SDL_PauseAudio(int on);
extern void SDL_Delay(uint32_t ms);
extern int SDL_Init(uint32_t flags);
extern uint32_t SDL_GetTicks(void);
extern int SDL_Quit(void);
extern int SDL_ShowSimpleMessageBox(uint32_t flags, const char *title, const char *message, void *window);
extern char *SDL_GetPrefPath(const char *org, const char *app);
#endif
