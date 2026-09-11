// SPDX-License-Identifier: GPL-3.0-or-later
#include "service.h"

#include "config.h"
#include "controller.h"
#include "lhm.h"
#include "log.h"
#include "nvapi.h"

#include <math.h>
#include <stdio.h>

static SERVICE_STATUS_HANDLE g_status_handle = NULL;
static SERVICE_STATUS g_status;
static HANDLE g_stop_event = NULL;
static HANDLE g_worker_thread = NULL;

static void report_status(DWORD state, DWORD win32_exit_code, DWORD wait_hint)
{
    static DWORD checkpoint = 1;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = win32_exit_code;
    g_status.dwWaitHint = wait_hint;
    g_status.dwCheckPoint = (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING)
                                ? checkpoint++ : 0;
    SetServiceStatus(g_status_handle, &g_status);
}

static BOOL config_and_log_paths(wchar_t *config_path, size_t config_count,
                                 wchar_t *log_path, size_t log_count)
{
    return config_get_path(config_path, config_count, FAN_MINITOOL_DEFAULT_CONFIG) &&
           config_get_path(log_path, log_count, FAN_MINITOOL_DEFAULT_LOG);
}

static DWORD WINAPI worker_main(void *unused)
{
    fan_config config;
    nvapi_context nvapi;
    lhm_client lhm;
    wchar_t config_path[MAX_PATH];
    wchar_t log_path[MAX_PATH];
    wchar_t error_text[256];
    double temperature_c;
    double pwm;
    double last_pwm = -1.0;
    BOOL log_ready = FALSE;
    BOOL lhm_ready = FALSE;
    BOOL nvapi_ready = FALSE;

    (void)unused;
    ZeroMemory(&config, sizeof(config));
    ZeroMemory(&nvapi, sizeof(nvapi));
    ZeroMemory(&lhm, sizeof(lhm));
    if (!config_and_log_paths(config_path, _countof(config_path), log_path, _countof(log_path))) {
        return 1;
    }
    log_ready = log_open(log_path);
    if (!log_ready) {
        OutputDebugStringW(L"fan-minitool: 无法打开日志文件\n");
    }

    error_text[0] = L'\0';
    if (!config_load(config_path, &config, error_text, _countof(error_text))) {
        log_write(L"配置加载失败: %s", error_text);
        goto cleanup;
    }
    log_write(L"服务启动，GPU=%lu，PWM SensorId=%s，轮询=%lu ms",
              config.nvidia_gpu_index, config.pwm_sensor_id, config.poll_interval_ms);
    if (!nvapi_open(&nvapi, error_text, _countof(error_text))) {
        log_write(L"NVAPI 初始化失败: %s", error_text);
        goto cleanup;
    }
    nvapi_ready = TRUE;
    if (!lhm_open(&lhm, &config)) {
        log_write(L"Libre Hardware Monitor HTTP 客户端初始化失败，错误码 %lu", GetLastError());
        goto cleanup;
    }
    lhm_ready = TRUE;

    // Until a valid GPU temperature is available, use the safe end of the curve.
    if (!lhm_set_pwm(&lhm, config.pwm_sensor_id, 100.0, error_text, _countof(error_text))) {
        log_write(L"无法设置安全 PWM=100: %s", error_text);
    } else {
        last_pwm = 100.0;
    }

    while (WaitForSingleObject(g_stop_event, 0) != WAIT_OBJECT_0) {
        error_text[0] = L'\0';
        if (!nvapi_get_gpu_temperature(&nvapi, config.nvidia_gpu_index, &temperature_c,
                                       error_text, _countof(error_text))) {
            log_write(L"读取 GPU 温度失败，维持安全 PWM=100: %s", error_text);
            pwm = 100.0;
        } else {
            pwm = controller_pwm_for_temperature(temperature_c, &config);
            log_write(L"GPU 温度 %.1f C，目标 PWM %.1f%%", temperature_c, pwm);
        }
        if (fabs(pwm - last_pwm) >= 0.1) {
            error_text[0] = L'\0';
            if (!lhm_set_pwm(&lhm, config.pwm_sensor_id, pwm,
                             error_text, _countof(error_text))) {
                log_write(L"设置主板 PWM %.1f%% 失败: %s", pwm, error_text);
            } else {
                last_pwm = pwm;
            }
        }
        if (WaitForSingleObject(g_stop_event, config.poll_interval_ms) == WAIT_OBJECT_0) {
            break;
        }
    }

cleanup:
    if (lhm_ready) {
        error_text[0] = L'\0';
        if (!lhm_release_pwm(&lhm, config.pwm_sensor_id, error_text, _countof(error_text))) {
            log_write(L"交还主板默认风扇曲线失败: %s", error_text);
        } else {
            log_write(L"已交还主板默认风扇曲线");
        }
        lhm_close(&lhm);
    }
    if (nvapi_ready) {
        nvapi_close(&nvapi);
    }
    if (log_ready) {
        log_write(L"服务线程退出");
        log_close();
    }
    return 0;
}

static DWORD WINAPI service_handler(DWORD control, DWORD event_type, void *event_data, void *context)
{
    (void)event_type;
    (void)event_data;
    (void)context;
    if ((control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) &&
        g_status.dwCurrentState == SERVICE_RUNNING) {
        report_status(SERVICE_STOP_PENDING, NO_ERROR, 10000);
        SetEvent(g_stop_event);
    }
    return NO_ERROR;
}

void WINAPI service_main(DWORD argc, LPWSTR *argv)
{
    (void)argc;
    (void)argv;
    ZeroMemory(&g_status, sizeof(g_status));
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_status_handle = RegisterServiceCtrlHandlerExW(FAN_MINITOOL_SERVICE_NAME,
                                                     service_handler, NULL);
    if (g_status_handle == NULL) {
        return;
    }
    report_status(SERVICE_START_PENDING, NO_ERROR, 10000);
    g_stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_stop_event == NULL) {
        report_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }
    g_worker_thread = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
    if (g_worker_thread == NULL) {
        CloseHandle(g_stop_event);
        g_stop_event = NULL;
        report_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }
    report_status(SERVICE_RUNNING, NO_ERROR, 0);
    WaitForSingleObject(g_worker_thread, INFINITE);
    CloseHandle(g_worker_thread);
    g_worker_thread = NULL;
    CloseHandle(g_stop_event);
    g_stop_event = NULL;
    report_status(SERVICE_STOPPED, NO_ERROR, 0);
}

BOOL service_run_console(void)
{
    HANDLE stop_event;
    HANDLE worker;

    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (stop_event == NULL) {
        return FALSE;
    }
    g_stop_event = stop_event;
    worker = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
    if (worker == NULL) {
        CloseHandle(stop_event);
        g_stop_event = NULL;
        return FALSE;
    }
    wprintf(L"fan-minitool 正在运行，按 Enter 停止。\n");
    (void)getchar();
    SetEvent(stop_event);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    CloseHandle(stop_event);
    g_stop_event = NULL;
    return TRUE;
}

BOOL service_install(void)
{
    SC_HANDLE manager = NULL;
    SC_HANDLE service = NULL;
    wchar_t executable[MAX_PATH];
    wchar_t binary_path[MAX_PATH + 32];
    BOOL result = FALSE;

    if (GetModuleFileNameW(NULL, executable, _countof(executable)) == 0 ||
        _snwprintf_s(binary_path, _countof(binary_path), _TRUNCATE,
                     L"\"%s\" --service", executable) < 0) {
        return FALSE;
    }
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (manager == NULL) {
        return FALSE;
    }
    service = CreateServiceW(manager, FAN_MINITOOL_SERVICE_NAME, FAN_MINITOOL_DISPLAY_NAME,
                             SERVICE_CHANGE_CONFIG | SERVICE_START | DELETE,
                             SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                             SERVICE_ERROR_NORMAL, binary_path, NULL, NULL, NULL, NULL, NULL);
    if (service != NULL) {
        result = TRUE;
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return result;
}

BOOL service_uninstall(void)
{
    SC_HANDLE manager;
    SC_HANDLE service;
    SERVICE_STATUS status;
    BOOL result;

    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (manager == NULL) {
        return FALSE;
    }
    service = OpenServiceW(manager, FAN_MINITOOL_SERVICE_NAME,
                           SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (service == NULL) {
        CloseServiceHandle(manager);
        return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST;
    }
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    result = DeleteService(service);
    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return result;
}
