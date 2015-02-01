/*
sousvided the Sous Vide deamon for the RaspberryPi
Copyright (C) 2015  Nima Saed-Samii

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef SOUSVIDED_MOTOR_H
#define SOUSVIDED_MOTOR_H

#include <stdint.h>

#define MOTOR_STATUS_OFF 0
#define MOTOR_STATUS_ON	1

struct motor {
	uint32_t pwm_clock_divider;
	uint32_t duty_cycle;
	uint32_t duty_cycle_range;
	uint8_t duty_cycle_changed;
	uint8_t status;
	uint8_t initialized;
};

typedef struct motor motor_t;

void motor_init(motor_t *m, const uint32_t pwm_clock_divider,
		const uint32_t duty_cycle_range);
void motor_cleanup(motor_t *);

void motor_set_duty_cycle(motor_t *m, const uint32_t duty_cycle);
void motor_set_duty_cycle_range(motor_t *m, const uint32_t duty_cycle_range);

uint32_t motor_get_duty_cycle(const motor_t *m);
uint32_t motor_get_duty_cycle_range(const motor_t *m);

float motor_get_duty_cycle_percentage(const motor_t *m);

int motor_get_status(const motor_t *m);
void motor_start(motor_t *m);
void motor_stop(motor_t *m);

#endif /* SOUSVIDED_MOTOR_H */
