// SPDX-License-Identifier: GPL-3.0-or-later
#include "service.h"
#include "config.h"

#include <stdio.h>
#include <wchar.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Winhttp.lib")

static void print_usage(void)
{
    wprintf(L"用法:\n");
    wprintf(L"  fan-minitool.exe install      安装并注册自动启动服务\n");
    wprintf(L"  fan-minitool.exe uninstall    停止并删除服务\n");
    wprintf(L"  fan-minitool.exe --console    前台运行，便于测试\n");
    wprintf(L"  fan-minitool.exe              由 SCM 启动服务\n");
}

int wmain(int argc, wchar_t **argv)
{
    if (argc > 1 && _wcsicmp(argv[1], L"install") == 0) {
        if (!service_install()) {
            wprintf(L"安装服务失败，错误码 %lu\n", GetLastError());
            return 1;
        }
        wprintf(L"服务已安装。\n");
        return 0;
    }
    if (argc > 1 && _wcsicmp(argv[1], L"uninstall") == 0) {
        if (!service_uninstall()) {
            wprintf(L"卸载服务失败，错误码 %lu\n", GetLastError());
            return 1;
        }
        wprintf(L"服务已卸载。\n");
        return 0;
    }
    if (argc > 1 && _wcsicmp(argv[1], L"--console") == 0) {
        return service_run_console() ? 0 : 1;
    }
    if (argc > 1 && _wcsicmp(argv[1], L"--help") == 0) {
        print_usage();
        return 0;
    }

    {
        SERVICE_TABLE_ENTRYW table[] = {
            { FAN_MINITOOL_SERVICE_NAME, service_main },
            { NULL, NULL }
        };
        if (!StartServiceCtrlDispatcherW(table)) {
            wprintf(L"启动服务调度失败，错误码 %lu\n", GetLastError());
            return 1;
        }
    }
    return 0;
}
