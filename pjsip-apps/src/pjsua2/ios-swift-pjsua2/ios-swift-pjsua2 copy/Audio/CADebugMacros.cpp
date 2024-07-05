/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
Part of CoreAudio utility classes. (Borrowed from aurioTouch sample code.)
*/

#include "CADebugMacros.h"
#include <stdio.h>
#include <stdarg.h>
#if TARGET_API_MAC_OSX
	#include <syslog.h>
#endif

#if DEBUG
#include <stdio.h>

void	DebugPrint(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
#endif // DEBUG

#if TARGET_API_MAC_OSX
void	LogError(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
#if DEBUG
	vprintf(fmt, args);
#endif
	vsyslog(LOG_ERR, fmt, args);
	va_end(args);
}

void	LogWarning(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
#if DEBUG
	vprintf(fmt, args);
#endif
	vsyslog(LOG_WARNING, fmt, args);
	va_end(args);
}
#endif
