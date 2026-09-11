// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FAN_MINITOOL_LHM_H
#define FAN_MINITOOL_LHM_H

#include "config.h"

typedef struct lhm_client {
    HINTERNET session;
    HINTERNET connection;
    wchar_t host[FAN_MINITOOL_MAX_HOST];
    INTERNET_PORT port;
} lhm_client;

BOOL lhm_open(lhm_client *client, const fan_config *config);
BOOL lhm_set_pwm(lhm_client *client, const wchar_t *sensor_id, double pwm_percent,
                 wchar_t *error_text, size_t error_count);
BOOL lhm_release_pwm(lhm_client *client, const wchar_t *sensor_id,
                     wchar_t *error_text, size_t error_count);
void lhm_close(lhm_client *client);

#endif
