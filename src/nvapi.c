// SPDX-License-Identifier: GPL-3.0-or-later
#include "nvapi.h"

#include <wchar.h>

#define NVAPI_ID_INITIALIZE 0x0150E828U
#define NVAPI_ID_UNLOAD 0xD22BDD7EU
#define NVAPI_ID_GET_ERROR_MESSAGE 0x6C2D048CU
#define NVAPI_ID_ENUM_PHYSICAL_GPUS 0xE5AC921FU
#define NVAPI_ID_GET_THERMAL_SETTINGS 0xE3640A56U
#define NVAPI_STATUS_OK 0

static BOOL set_error(wchar_t *error_text, size_t error_count, const wchar_t *message)
{
    if (error_text != NULL && error_count > 0) {
        wcsncpy_s(error_text, error_count, message, _TRUNCATE);
    }
    return FALSE;
}

static BOOL set_status_error(const nvapi_context *context, nvapi_status status,
                             wchar_t *error_text, size_t error_count)
{
    char message[64] = "";
    wchar_t wide_message[128];

    if (context != NULL && context->get_error_message != NULL &&
        context->get_error_message(status, message) == NVAPI_STATUS_OK &&
        MultiByteToWideChar(CP_ACP, 0, message, -1, wide_message,
                            (int)_countof(wide_message)) > 0) {
        if (error_text != NULL && error_count > 0) {
            _snwprintf_s(error_text, error_count, _TRUNCATE,
                         L"NVAPI 错误 %d: %s", status, wide_message);
        }
        return FALSE;
    }
    if (error_text != NULL && error_count > 0) {
        _snwprintf_s(error_text, error_count, _TRUNCATE, L"NVAPI 错误 %d", status);
    }
    return FALSE;
}

static void *query(nvapi_query_interface_fn query_interface, uint32_t id)
{
    return query_interface == NULL ? NULL : query_interface(id);
}

BOOL nvapi_open(nvapi_context *context, wchar_t *error_text, size_t error_count)
{
    HMODULE module;
    nvapi_query_interface_fn query_interface;
    nvapi_initialize_fn initialize;
    nvapi_status status;

    if (context == NULL) {
        return set_error(error_text, error_count, L"NVAPI 上下文为空");
    }
    ZeroMemory(context, sizeof(*context));

    module = LoadLibraryExW(L"nvapi64.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module == NULL) {
        module = LoadLibraryW(L"nvapi64.dll");
    }
    if (module == NULL) {
        return set_error(error_text, error_count, L"找不到 NVIDIA 驱动提供的 nvapi64.dll");
    }
    query_interface = (nvapi_query_interface_fn)GetProcAddress(module, "nvapi_QueryInterface");
    if (query_interface == NULL) {
        FreeLibrary(module);
        return set_error(error_text, error_count, L"nvapi_QueryInterface 不可用");
    }

    initialize = (nvapi_initialize_fn)query(query_interface, NVAPI_ID_INITIALIZE);
    context->unload = (nvapi_unload_fn)query(query_interface, NVAPI_ID_UNLOAD);
    context->get_error_message = (nvapi_get_error_message_fn)query(query_interface, NVAPI_ID_GET_ERROR_MESSAGE);
    context->enum_physical_gpus = (nvapi_enum_physical_gpus_fn)query(query_interface, NVAPI_ID_ENUM_PHYSICAL_GPUS);
    context->get_thermal_settings =
        (nvapi_get_thermal_settings_fn)query(query_interface, NVAPI_ID_GET_THERMAL_SETTINGS);
    if (initialize == NULL || context->unload == NULL || context->enum_physical_gpus == NULL ||
        context->get_thermal_settings == NULL) {
        FreeLibrary(module);
        ZeroMemory(context, sizeof(*context));
        return set_error(error_text, error_count, L"NVIDIA NVAPI 接口不完整");
    }

    status = initialize();
    if (status != NVAPI_STATUS_OK) {
        set_status_error(context, status, error_text, error_count);
        FreeLibrary(module);
        ZeroMemory(context, sizeof(*context));
        return FALSE;
    }
    context->module = module;
    context->initialized = TRUE;
    status = context->enum_physical_gpus(context->handles, &context->gpu_count);
    if (status != NVAPI_STATUS_OK || context->gpu_count == 0) {
        set_status_error(context, status, error_text, error_count);
        nvapi_close(context);
        return FALSE;
    }
    return TRUE;
}

BOOL nvapi_get_gpu_temperature(const nvapi_context *context, DWORD gpu_index, double *temperature_c,
                               wchar_t *error_text, size_t error_count)
{
    nv_gpu_thermal_settings settings;
    nvapi_status status;
    uint32_t i;
    uint32_t sensor_count;
    const nv_thermal_sensor *selected = NULL;

    if (context == NULL || temperature_c == NULL || !context->initialized) {
        return set_error(error_text, error_count, L"NVAPI 尚未初始化");
    }
    if (gpu_index >= context->gpu_count) {
        return set_error(error_text, error_count, L"配置中的 NVIDIA GPU 编号不存在");
    }
    ZeroMemory(&settings, sizeof(settings));
    settings.version = (uint32_t)(sizeof(settings) | (2U << 16));
    status = context->get_thermal_settings(context->handles[gpu_index],
                                            NVAPI_THERMAL_TARGET_ALL, &settings);
    if (status != NVAPI_STATUS_OK) {
        return set_status_error(context, status, error_text, error_count);
    }
    sensor_count = settings.count;
    if (sensor_count > NVAPI_MAX_THERMAL_SENSORS_PER_GPU) {
        sensor_count = NVAPI_MAX_THERMAL_SENSORS_PER_GPU;
    }
    for (i = 0; i < sensor_count; ++i) {
        if (settings.sensor[i].target == NVAPI_THERMAL_TARGET_GPU) {
            selected = &settings.sensor[i];
            break;
        }
    }
    if (selected == NULL && sensor_count > 0) {
        selected = &settings.sensor[0];
    }
    if (selected == NULL) {
        return set_error(error_text, error_count, L"NVIDIA GPU 没有可用的温度传感器");
    }
    *temperature_c = (double)selected->current_temp;
    return TRUE;
}

void nvapi_close(nvapi_context *context)
{
    if (context == NULL) {
        return;
    }
    if (context->initialized && context->unload != NULL) {
        context->unload();
    }
    if (context->module != NULL) {
        FreeLibrary(context->module);
    }
    ZeroMemory(context, sizeof(*context));
}
