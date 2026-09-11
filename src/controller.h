// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FAN_MINITOOL_CONTROLLER_H
#define FAN_MINITOOL_CONTROLLER_H

#include "config.h"

double controller_pwm_for_temperature(double temperature_c, const fan_config *config);

#endif
