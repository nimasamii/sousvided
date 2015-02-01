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

#include "motor.h"

#include <assert.h>
#include <stddef.h>

#include "bcm2835.h"

#define MOTOR_PWM_PIN RPI_V2_GPIO_P1_12
#define MOTOR_PWM_CHANNEL 0
#define MOTOR_MARKSPACE_MODE 1

void motor_init(motor_t *m, const uint32_t pwm_clock_divider,
		const uint32_t duty_cycle_range)
{
	assert(m != NULL);
	assert(!m->initialized);

	/* clock divider can only be even */
	m->pwm_clock_divider = pwm_clock_divider & ~(0x00000001);

	m->duty_cycle = 0;
	m->duty_cycle_range = duty_cycle_range;
	m->status = MOTOR_STATUS_OFF;

	/* set alternate function 5 for pin to provide PWM output */
	bcm2835_gpio_fsel(MOTOR_PWM_PIN, BCM2835_GPIO_FSEL_ALT5);

	/* set PWM clock divider to select PWM frequency (base clock is
	 * 19.2 Mhz) */
	bcm2835_pwm_set_clock(m->pwm_clock_divider);

	/* set MARKSPACE mode and provide no PWM output */
	bcm2835_pwm_set_mode(MOTOR_PWM_CHANNEL, MOTOR_MARKSPACE_MODE, 0);

	/* set duty cycle range */
	bcm2835_pwm_set_range(MOTOR_PWM_CHANNEL, duty_cycle_range);

	/* PWM is off, but still set the current duty cycle to zero */
	bcm2835_pwm_set_data(MOTOR_PWM_CHANNEL, 0);
}

void motor_cleanup(motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);

	/* set duty cycle to 0 to stop motor */
	bcm2835_pwm_set_data(MOTOR_PWM_CHANNEL, 0);

	/* disable PWM if it is currently active */
	if (m->status == MOTOR_STATUS_ON) {
		bcm2835_pwm_set_mode(MOTOR_PWM_CHANNEL, MOTOR_MARKSPACE_MODE, 0);
		m->status = MOTOR_STATUS_OFF;
	}

	/* return GPIO pin to input state */
	bcm2835_gpio_fsel(MOTOR_PWM_PIN, BCM2835_GPIO_FSEL_INPT);

	m->duty_cycle = 0;
	m->duty_cycle_range = 0;
	m->initialized = 0;
}

void motor_set_duty_cycle(motor_t *m, const uint32_t duty_cycle)
{
	assert(m != NULL);
	assert(m->initialized);
	if (m->duty_cycle_range < duty_cycle) {
		m->duty_cycle = m->duty_cycle_range;
	} else {
		m->duty_cycle = duty_cycle;
	}

	if (m->status == MOTOR_STATUS_ON) {
		bcm2835_pwm_set_data(MOTOR_PWM_CHANNEL, m->duty_cycle);
	}
}

void motor_set_duty_cycle_range(motor_t *m, const uint32_t duty_cycle_range)
{
	assert(m != NULL);
	assert(m->initialized);
	assert(duty_cycle_range > 10);

	const float percentage = motor_get_duty_cycle_percentage(m);
	m->duty_cycle = percentage * duty_cycle_range;
	m->duty_cycle_range = duty_cycle_range;

	if (m->status == MOTOR_STATUS_OFF) {
		m->duty_cycle_changed = 1;
	} else if (m->duty_cycle > duty_cycle_range) {
		/* update duty cycle first, then set new range */
		bcm2835_pwm_set_data(MOTOR_PWM_CHANNEL, m->duty_cycle);
		bcm2835_pwm_set_range(MOTOR_PWM_CHANNEL, m->duty_cycle_range);
	} else {
		/* set new range first, then update duty cycle */
		bcm2835_pwm_set_range(MOTOR_PWM_CHANNEL, m->duty_cycle_range);
		bcm2835_pwm_set_data(MOTOR_PWM_CHANNEL, m->duty_cycle);
	}
}

uint32_t motor_get_duty_cycle(const motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);
	return m->duty_cycle;
}

uint32_t motor_get_duty_cycle_range(const motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);
	return m->duty_cycle_range;
}

float motor_get_duty_cycle_percentage(const motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);
	return ((float)m->duty_cycle / m->duty_cycle_range);
}

int motor_get_status(const motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);
	if (m->status == MOTOR_STATUS_OFF ||
	    motor_get_duty_cycle_percentage(m) < 8.0) {
		return MOTOR_STATUS_OFF;
	}
	return MOTOR_STATUS_ON;
}

void motor_start(motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);

	if (m->status != MOTOR_STATUS_ON) {
		bcm2835_pwm_set_mode(MOTOR_PWM_CHANNEL, MOTOR_MARKSPACE_MODE,
				     MOTOR_STATUS_ON);
		m->status = MOTOR_STATUS_ON;

		if (m->duty_cycle_changed) {
			bcm2835_pwm_set_range(MOTOR_PWM_CHANNEL,
					      m->duty_cycle_range);
			m->duty_cycle_changed = 0;
		}
		bcm2835_pwm_set_data(MOTOR_PWM_CHANNEL, m->duty_cycle);
	}
}

void motor_stop(motor_t *m)
{
	assert(m != NULL);
	assert(m->initialized);

	if (m->status != MOTOR_STATUS_OFF) {
		bcm2835_pwm_set_mode(MOTOR_PWM_CHANNEL, MOTOR_MARKSPACE_MODE,
				     MOTOR_STATUS_OFF);
		m->status = MOTOR_STATUS_OFF;
	}
}
