// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FAN_MINITOOL_CONFIG_H
#define FAN_MINITOOL_CONFIG_H

#include <windows.h>
#include <winhttp.h>
#include <stddef.h>

#define FAN_MINITOOL_SERVICE_NAME L"fan-minitool"
#define FAN_MINITOOL_DISPLAY_NAME L"fan-minitool GPU temperature fan controller"
#define FAN_MINITOOL_DEFAULT_CONFIG L"fan-minitool.ini"
#define FAN_MINITOOL_DEFAULT_LOG L"fan-minitool.log"
#define FAN_MINITOOL_MAX_SENSOR_ID 512
#define FAN_MINITOOL_MAX_HOST 128

typedef struct fan_config {
    DWORD nvidia_gpu_index;
    DWORD poll_interval_ms;
    double start_temperature_c;
    double max_temperature_c;
    double start_pwm_percent;
    wchar_t lhm_host[FAN_MINITOOL_MAX_HOST];
    INTERNET_PORT lhm_port;
    wchar_t pwm_sensor_id[FAN_MINITOOL_MAX_SENSOR_ID];
} fan_config;

BOOL config_load(const wchar_t *path, fan_config *config, wchar_t *error_text, size_t error_count);
BOOL config_get_path(wchar_t *path, size_t path_count, const wchar_t *file_name);

#endif
