// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller.h"

double controller_pwm_for_temperature(double temperature_c, const fan_config *config)
{
    double fraction;

    if (config == NULL || temperature_c < config->start_temperature_c) {
        return 0.0;
    }
    if (temperature_c >= config->max_temperature_c) {
        return 100.0;
    }
    fraction = (temperature_c - config->start_temperature_c) /
               (config->max_temperature_c - config->start_temperature_c);
    return config->start_pwm_percent +
           fraction * (100.0 - config->start_pwm_percent);
}
