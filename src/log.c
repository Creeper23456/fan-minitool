// SPDX-License-Identifier: GPL-3.0-or-later
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static HANDLE g_log_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_log_lock;
static BOOL g_log_lock_initialized = FALSE;

BOOL log_open(const wchar_t *path)
{
    if (path == NULL) {
        return FALSE;
    }
    InitializeCriticalSection(&g_log_lock);
    g_log_lock_initialized = TRUE;
    g_log_file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return g_log_file != INVALID_HANDLE_VALUE;
}

void log_close(void)
{
    if (g_log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log_file);
        g_log_file = INVALID_HANDLE_VALUE;
    }
    if (g_log_lock_initialized) {
        DeleteCriticalSection(&g_log_lock);
        g_log_lock_initialized = FALSE;
    }
}

void log_write(const wchar_t *format, ...)
{
    wchar_t message[2048];
    wchar_t line[2304];
    char utf8[4608];
    SYSTEMTIME now;
    va_list args;
    int line_length;
    int utf8_length;
    DWORD written;

    if (format == NULL) {
        return;
    }
    va_start(args, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);
    va_end(args);

    GetLocalTime(&now);
    line_length = _snwprintf_s(line, _countof(line), _TRUNCATE,
                               L"%04u-%02u-%02u %02u:%02u:%02u.%03u %s\r\n",
                               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                               now.wSecond, now.wMilliseconds, message);
    if (line_length <= 0) {
        return;
    }

    OutputDebugStringW(line);
    if (g_log_file == INVALID_HANDLE_VALUE || !g_log_lock_initialized) {
        return;
    }
    utf8_length = WideCharToMultiByte(CP_UTF8, 0, line, line_length, utf8,
                                      (int)sizeof(utf8), NULL, NULL);
    if (utf8_length <= 0) {
        return;
    }
    EnterCriticalSection(&g_log_lock);
    WriteFile(g_log_file, utf8, (DWORD)utf8_length, &written, NULL);
    LeaveCriticalSection(&g_log_lock);
}
