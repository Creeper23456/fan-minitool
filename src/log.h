// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FAN_MINITOOL_LOG_H
#define FAN_MINITOOL_LOG_H

#include <windows.h>

BOOL log_open(const wchar_t *path);
void log_close(void);
void log_write(const wchar_t *format, ...);

#endif
