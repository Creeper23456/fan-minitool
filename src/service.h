// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FAN_MINITOOL_SERVICE_H
#define FAN_MINITOOL_SERVICE_H

#include <windows.h>

void WINAPI service_main(DWORD argc, LPWSTR *argv);
BOOL service_run_console(void);
BOOL service_install(void);
BOOL service_uninstall(void);

#endif
