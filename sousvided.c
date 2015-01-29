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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcm2835.h"
#include "buttons.h"
#include "max31865.h"
#include "motor.h"
#include "pid.h"
#include "rtd_table.h"

#define MAX31865_DRDY_PIN RPI_V2_GPIO_P1_22

#define MOTOR_CLOCK_DIVIDER BCM2835_PWM_CLOCK_DIVIDER_1024
#define MOTOR_PWM_RANGE 1000
#define MOTOR_SPEED_DELTA 50

#define PID_UPDATE_FREQUENCY_MS 1000
#define PID_MIN_DUTY_CYCLE 0.02
#define PID_MAX_DUTY_CYCLE 1.0
#define PID_MIN_SET_POINT 20.0
#define PID_MAX_SET_POINT 95.0
#define PID_SET_POINT_DELTA 0.5

#define BUTTON_1_PIN RPI_V2_GPIO_P1_13
#define BUTTON_2_PIN RPI_V2_GPIO_P1_15
#define BUTTON_3_PIN RPI_V2_GPIO_P1_16
#define BUTTON_4_PIN RPI_V2_GPIO_P1_18

static double query_wrapper(void *p)
{
	return max31865_get_temperature((max31865_t *)p, NULL);
}

struct buttons_callback_data {
	pidctrl_t *pidctrl;
	motor_t *motor;
};

static void change_motor_speed(motor_t *motor, int32_t delta)
{
	uint32_t duty_cycle = motor_get_duty_cycle(motor);
	if (delta < 0) {
		if ((uint32_t)abs(delta) > duty_cycle) {
			duty_cycle = 0;
		} else {
			duty_cycle += delta;
		}
	} else {
		if (duty_cycle + delta > motor_get_duty_cycle_range(motor)) {
			duty_cycle = motor_get_duty_cycle_range(motor);
		} else {
			duty_cycle += delta;
		}
	}
	motor_set_duty_cycle(motor, duty_cycle);
	printf("New motor speed is %f%%\n",
	       motor_get_duty_cycle_percentage(motor));
}

static void update_target_temperature(pidctrl_t *pidctrl, double delta)
{
	double current = pidctrl_get_set_point(pidctrl);
	if (delta < 0 && current + delta < PID_MIN_SET_POINT) {
		current = PID_MIN_SET_POINT;
	} else if (delta > 0 && current + delta > PID_MAX_SET_POINT) {
		current = PID_MAX_SET_POINT;
	} else {
		current += delta;
	}
	pidctrl_set_set_point(pidctrl, current);
	printf("New target temperature %f degree Celsius\n", current);
}

static void button_callback_handler(const uint8_t pin, void *user_data)
{
	struct buttons_callback_data *data =
	    (struct buttons_callback_data *)user_data;

	switch (pin) {
	case BUTTON_1_PIN:
		/* increment temperature set point in PID controller */
		update_target_temperature(data->pidctrl, PID_SET_POINT_DELTA);
		break;
	case BUTTON_2_PIN:
		/* decrement temperature set point in PID controller */
		update_target_temperature(data->pidctrl, -PID_SET_POINT_DELTA);
		break;
	case BUTTON_3_PIN:
		change_motor_speed(data->motor, MOTOR_SPEED_DELTA);
		break;
	case BUTTON_4_PIN:
		change_motor_speed(data->motor, -MOTOR_SPEED_DELTA);
		break;
		break;
	default:
		/* invalid button pin ID */
		assert(0);
	}
}

int main(int argc, char **argv)
{
	/************************************************************************
	 * TODO: read RTD table initalizer values and MAX31865 config parameters
	 *       from config file (libconfig?)
	 ***********************************************************************/

	max31865_t maxim;
	motor_t motor;
	pidctrl_t *pidctrl = NULL;
	buttons_t *btns = NULL;
	struct buttons_callback_data btn_cb_data = { NULL, NULL };

	if (!bcm2835_init()) {
		fprintf(stderr, "Failed to initialize bcm2835 library.\n");
		exit(EXIT_FAILURE);
	}

	printf("Initializing RTD table\n");
	if (init_rtd_table(0.0, 200.0, 0.1, 1000.0, 3600.0) == -1) {
		bcm2835_close();
		exit(EXIT_FAILURE);
	}

	memset(&maxim, 0, sizeof(maxim));
	max31865_init(&maxim, BCM2835_SPI_CS0, MAX31865_DRDY_PIN,
		      MAX31865_4WIRE_RTD);

	memset(&motor, 0, sizeof(motor));
	motor_init(&motor, MOTOR_CLOCK_DIVIDER, MOTOR_PWM_RANGE);

	pidctrl = pidctrl_init(40.0, 0.5, 25.0, 4.0, &query_wrapper, &maxim,
			       PID_UPDATE_FREQUENCY_MS, PID_MIN_DUTY_CYCLE,
			       PID_MAX_DUTY_CYCLE);
	if (!pidctrl) {
		fprintf(stderr, "Failed to initialize PID controller.\n");
		motor_cleanup(&motor);
		max31865_cleanup(&maxim);
		free_rtd_table();
		bcm2835_close();
		exit(EXIT_FAILURE);
	}

	btn_cb_data.pidctrl = pidctrl;
	btn_cb_data.motor = &motor;
	btns =
	    buttons_init(BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN, BUTTON_4_PIN,
			 50, &button_callback_handler, (void *)&btn_cb_data);
	if (!btns) {
		fprintf(stderr, "Failed to initialize button handler.\n");
		pidctrl_free(pidctrl);
		motor_cleanup(&motor);
		max31865_cleanup(&maxim);
		free_rtd_table();
		bcm2835_close();
		exit(EXIT_FAILURE);
	}

	buttons_cleanup(btns);
	pidctrl_free(pidctrl);
	motor_cleanup(&motor);
	max31865_cleanup(&maxim);
	free_rtd_table();
	bcm2835_close();
	return 0;
}
