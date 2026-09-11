// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal ABI declarations for the NVIDIA NVAPI calls used by fan-minitool.
// The NVIDIA implementation is loaded from the installed display driver at
// runtime; no NVIDIA SDK binary is distributed with this project.
#ifndef FAN_MINITOOL_NVAPI_H
#define FAN_MINITOOL_NVAPI_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#define NVAPI_MAX_PHYSICAL_GPUS 64
#define NVAPI_MAX_THERMAL_SENSORS_PER_GPU 3
#define NVAPI_THERMAL_TARGET_ALL 15U
#define NVAPI_THERMAL_TARGET_GPU 1

typedef int32_t nvapi_status;
typedef void *nv_physical_gpu_handle;

typedef struct nv_thermal_sensor {
    int32_t controller;
    int32_t default_min_temp;
    int32_t default_max_temp;
    int32_t current_temp;
    int32_t target;
} nv_thermal_sensor;

#pragma pack(push, 8)
typedef struct nv_gpu_thermal_settings {
    uint32_t version;
    uint32_t count;
    nv_thermal_sensor sensor[NVAPI_MAX_THERMAL_SENSORS_PER_GPU];
} nv_gpu_thermal_settings;
#pragma pack(pop)

typedef void *(__cdecl *nvapi_query_interface_fn)(uint32_t id);
typedef nvapi_status(__cdecl *nvapi_initialize_fn)(void);
typedef nvapi_status(__cdecl *nvapi_unload_fn)(void);
typedef nvapi_status(__cdecl *nvapi_get_error_message_fn)(nvapi_status status, char *message);
typedef nvapi_status(__cdecl *nvapi_enum_physical_gpus_fn)(
    nv_physical_gpu_handle handles[NVAPI_MAX_PHYSICAL_GPUS], uint32_t *count);
typedef nvapi_status(__cdecl *nvapi_get_thermal_settings_fn)(
    nv_physical_gpu_handle handle, uint32_t sensor_index, nv_gpu_thermal_settings *settings);

typedef struct nvapi_context {
    HMODULE module;
    nvapi_unload_fn unload;
    nvapi_get_error_message_fn get_error_message;
    nvapi_enum_physical_gpus_fn enum_physical_gpus;
    nvapi_get_thermal_settings_fn get_thermal_settings;
    nv_physical_gpu_handle handles[NVAPI_MAX_PHYSICAL_GPUS];
    uint32_t gpu_count;
    BOOL initialized;
} nvapi_context;

BOOL nvapi_open(nvapi_context *context, wchar_t *error_text, size_t error_count);
BOOL nvapi_get_gpu_temperature(const nvapi_context *context, DWORD gpu_index, double *temperature_c,
                               wchar_t *error_text, size_t error_count);
void nvapi_close(nvapi_context *context);

#endif
