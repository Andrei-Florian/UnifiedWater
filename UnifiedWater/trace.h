#ifndef _TRACE_H_
#define _TRACE_H_

#ifdef DEBUG
#define __TRACE__(msg)  Serial.println(msg)
#define __TRACEF__(fmt, ...)  Serial_printf(fmt, __VA_ARGS__)
#else
#define __TRACE__(msg)       {}
#define __TRACEF__(fmt, ...)  {}
#endif

#if USE_DISPLAY
#include "./graphics/contoso_display.h"
#ifdef DEBUG
#define __TRACE__(msg)  drawMessage(msg)
#define __TRACEF__(fmt, ...)  drawMessageF(fmt, __VA_ARGS__)
#endif
#else
#define __REFRESH_DISPLAY__   {}
#endif

#endif