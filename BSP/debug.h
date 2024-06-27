#ifndef BSP_DEBUG_H
#define BSP_DEBUG_H

#include "SEGGER_RTT.h"

#define	debug_printf(...)	SEGGER_RTT_printf(0, __VA_ARGS__)
#endif
