// SPDX-License-Identifier: GPL-3.0-or-later
#include "lhm.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static BOOL set_error(wchar_t *error_text, size_t error_count, const wchar_t *message)
{
    if (error_text != NULL && error_count > 0) {
        wcsncpy_s(error_text, error_count, message, _TRUNCATE);
    }
    return FALSE;
}

static BOOL is_query_safe(wchar_t value)
{
    return (value >= L'a' && value <= L'z') || (value >= L'A' && value <= L'Z') ||
           (value >= L'0' && value <= L'9') || value == L'/' || value == L'_' ||
           value == L'-' || value == L'.';
}

static BOOL append_encoded(wchar_t *output, size_t output_count, const wchar_t *input)
{
    size_t used = 0;
    size_t i;

    for (i = 0; input[i] != L'\0'; ++i) {
        if (is_query_safe(input[i])) {
            if (used + 1 >= output_count) {
                return FALSE;
            }
            output[used++] = input[i];
        } else {
            if (used + 3 >= output_count || input[i] > 0x7f) {
                return FALSE;
            }
            _snwprintf_s(output + used, output_count - used, _TRUNCATE,
                         L"%%%02X", (unsigned int)input[i]);
            used += 3;
        }
    }
    output[used] = L'\0';
    return TRUE;
}

static BOOL response_is_ok(HINTERNET request)
{
    char response[1024];
    DWORD available;
    DWORD read_count;
    size_t used = 0;

    response[0] = '\0';
    for (;;) {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            return FALSE;
        }
        if (available == 0) {
            break;
        }
        if (available > sizeof(response) - 1 - used) {
            available = (DWORD)(sizeof(response) - 1 - used);
        }
        if (available == 0 || !WinHttpReadData(request, response + used, available, &read_count)) {
            return FALSE;
        }
        used += read_count;
        response[used] = '\0';
        if (used == sizeof(response) - 1) {
            break;
        }
    }
    return strstr(response, "\"result\":\"ok\"") != NULL ||
           strstr(response, "\"result\": \"ok\"") != NULL;
}

static BOOL request_sensor(lhm_client *client, const wchar_t *sensor_id,
                           const wchar_t *action, const wchar_t *value,
                           wchar_t *error_text, size_t error_count)
{
    HINTERNET request = NULL;
    wchar_t encoded_id[FAN_MINITOOL_MAX_SENSOR_ID * 3];
    wchar_t object_name[FAN_MINITOOL_MAX_SENSOR_ID * 3 + 128];
    wchar_t value_text[64];
    BOOL result = FALSE;

    if (client == NULL || client->connection == NULL || sensor_id == NULL || action == NULL) {
        return set_error(error_text, error_count, L"Libre Hardware Monitor 客户端未初始化");
    }
    if (!append_encoded(encoded_id, _countof(encoded_id), sensor_id)) {
        return set_error(error_text, error_count, L"PWM SensorId 太长或包含不支持的字符");
    }
    if (value == NULL) {
        wcscpy_s(value_text, _countof(value_text), L"null");
    } else {
        wcscpy_s(value_text, _countof(value_text), value);
    }
    if (_snwprintf_s(object_name, _countof(object_name), _TRUNCATE,
                     L"/Sensor?id=%s&action=%s&value=%s", encoded_id, action, value_text) < 0) {
        return set_error(error_text, error_count, L"Libre Hardware Monitor 请求过长");
    }

    request = WinHttpOpenRequest(client->connection, L"POST", object_name, NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 WINHTTP_FLAG_REFRESH);
    if (request == NULL) {
        if (error_text != NULL && error_count > 0) {
            _snwprintf_s(error_text, error_count, _TRUNCATE,
                         L"WinHttpOpenRequest 失败，错误码 %lu", GetLastError());
        }
        return FALSE;
    }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, NULL)) {
        if (error_text != NULL && error_count > 0) {
            _snwprintf_s(error_text, error_count, _TRUNCATE,
                         L"Libre Hardware Monitor 请求失败，错误码 %lu", GetLastError());
        }
        WinHttpCloseHandle(request);
        return FALSE;
    }
    result = response_is_ok(request);
    if (!result) {
        set_error(error_text, error_count, L"Libre Hardware Monitor 返回失败");
    }
    WinHttpCloseHandle(request);
    return result;
}

BOOL lhm_open(lhm_client *client, const fan_config *config)
{
    if (client == NULL || config == NULL) {
        return FALSE;
    }
    ZeroMemory(client, sizeof(*client));
    wcsncpy_s(client->host, _countof(client->host), config->lhm_host, _TRUNCATE);
    client->port = config->lhm_port;
    client->session = WinHttpOpen(L"fan-minitool/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (client->session == NULL) {
        return FALSE;
    }
    if (!WinHttpSetTimeouts(client->session, 2000, 2000, 2000, 2000)) {
        WinHttpCloseHandle(client->session);
        client->session = NULL;
        return FALSE;
    }
    client->connection = WinHttpConnect(client->session, client->host, client->port, 0);
    if (client->connection == NULL) {
        WinHttpCloseHandle(client->session);
        client->session = NULL;
        return FALSE;
    }
    return TRUE;
}

BOOL lhm_set_pwm(lhm_client *client, const wchar_t *sensor_id, double pwm_percent,
                 wchar_t *error_text, size_t error_count)
{
    wchar_t value[64];
    _snwprintf_s(value, _countof(value), _TRUNCATE, L"%.2f", pwm_percent);
    return request_sensor(client, sensor_id, L"Set", value, error_text, error_count);
}

BOOL lhm_release_pwm(lhm_client *client, const wchar_t *sensor_id,
                     wchar_t *error_text, size_t error_count)
{
    return request_sensor(client, sensor_id, L"Set", NULL, error_text, error_count);
}

void lhm_close(lhm_client *client)
{
    if (client == NULL) {
        return;
    }
    if (client->connection != NULL) {
        WinHttpCloseHandle(client->connection);
    }
    if (client->session != NULL) {
        WinHttpCloseHandle(client->session);
    }
    ZeroMemory(client, sizeof(*client));
}
