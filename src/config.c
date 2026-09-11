// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"

#include <stdlib.h>
#include <wchar.h>

static BOOL set_error(wchar_t *error_text, size_t error_count, const wchar_t *message)
{
    if (error_text != NULL && error_count > 0) {
        wcsncpy_s(error_text, error_count, message, _TRUNCATE);
    }
    return FALSE;
}

static BOOL read_double(const wchar_t *path, const wchar_t *section, const wchar_t *key,
                        double default_value, double *value)
{
    wchar_t text[64];
    wchar_t *end = NULL;
    double parsed;

    GetPrivateProfileStringW(section, key, L"", text, (DWORD)_countof(text), path);
    if (text[0] == L'\0') {
        *value = default_value;
        return TRUE;
    }
    parsed = wcstod(text, &end);
    if (end == text || *end != L'\0') {
        return FALSE;
    }
    *value = parsed;
    return TRUE;
}

static BOOL file_exists(const wchar_t *path)
{
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

BOOL config_load(const wchar_t *path, fan_config *config, wchar_t *error_text, size_t error_count)
{
    wchar_t sensor_id[FAN_MINITOOL_MAX_SENSOR_ID];
    DWORD port;

    if (path == NULL || config == NULL) {
        return set_error(error_text, error_count, L"配置参数为空");
    }
    if (!file_exists(path)) {
        return set_error(error_text, error_count, L"配置文件不存在");
    }

    ZeroMemory(config, sizeof(*config));
    config->nvidia_gpu_index = GetPrivateProfileIntW(L"nvidia", L"gpu_index", 0, path);
    config->poll_interval_ms = GetPrivateProfileIntW(L"control", L"poll_interval_ms", 2000, path);
    port = GetPrivateProfileIntW(L"librehardwaremonitor", L"port", 8085, path);
    GetPrivateProfileStringW(L"librehardwaremonitor", L"host", L"127.0.0.1",
                             config->lhm_host, (DWORD)_countof(config->lhm_host), path);
    GetPrivateProfileStringW(L"librehardwaremonitor", L"pwm_sensor_id", L"",
                             sensor_id, (DWORD)_countof(sensor_id), path);
    wcsncpy_s(config->pwm_sensor_id, _countof(config->pwm_sensor_id), sensor_id, _TRUNCATE);

    if (!read_double(path, L"control", L"start_temperature_c", 50.0,
                     &config->start_temperature_c) ||
        !read_double(path, L"control", L"max_temperature_c", 85.0,
                     &config->max_temperature_c) ||
        !read_double(path, L"control", L"start_pwm_percent", 30.0,
                     &config->start_pwm_percent)) {
        return set_error(error_text, error_count, L"温度或 PWM 数值格式错误");
    }

    if (config->poll_interval_ms < 250 || config->poll_interval_ms > 600000) {
        return set_error(error_text, error_count, L"poll_interval_ms 必须在 250 到 600000 之间");
    }
    if (port == 0 || port > 65535) {
        return set_error(error_text, error_count, L"Libre Hardware Monitor 端口无效");
    }
    config->lhm_port = (INTERNET_PORT)port;
    if (config->pwm_sensor_id[0] == L'\0') {
        return set_error(error_text, error_count, L"未配置 librehardwaremonitor.pwm_sensor_id");
    }
    if (config->start_temperature_c >= config->max_temperature_c) {
        return set_error(error_text, error_count, L"start_temperature_c 必须小于 max_temperature_c");
    }
    if (config->start_pwm_percent < 0.0 || config->start_pwm_percent > 100.0) {
        return set_error(error_text, error_count, L"start_pwm_percent 必须在 0 到 100 之间");
    }

    return TRUE;
}

BOOL config_get_path(wchar_t *path, size_t path_count, const wchar_t *file_name)
{
    DWORD length;
    wchar_t *separator;

    if (path == NULL || path_count == 0 || file_name == NULL) {
        return FALSE;
    }
    length = GetModuleFileNameW(NULL, path, (DWORD)path_count);
    if (length == 0 || length >= path_count) {
        return FALSE;
    }
    separator = wcsrchr(path, L'\\');
    if (separator == NULL) {
        separator = wcsrchr(path, L'/');
    }
    if (separator == NULL) {
        return FALSE;
    }
    separator[1] = L'\0';
    return wcscat_s(path, path_count, file_name) == 0;
}
