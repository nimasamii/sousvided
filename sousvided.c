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
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

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

#define PID_MIN_DUTY_CYCLE 0.02
#define PID_MAX_DUTY_CYCLE 1.0
#define PID_MIN_SET_POINT 20.0
#define PID_MAX_SET_POINT 95.0
#define PID_SET_POINT_DELTA 0.5
#define PID_CONTROL_LOOP_HZ 5
#define PID_CONTROL_LOOP_MS (1000 / PID_CONTROL_LOOP_HZ)

#define BUTTON_1_PIN RPI_V2_GPIO_P1_13
#define BUTTON_2_PIN RPI_V2_GPIO_P1_15
#define BUTTON_3_PIN RPI_V2_GPIO_P1_16
#define BUTTON_4_PIN RPI_V2_GPIO_P1_18

#define SSR_PIN RPI_V2_GPIO_P1_07

struct callback_data {
	max31865_t maxim;
	motor_t motor;
	pidctrl_t *pidctrl;
	buttons_t *buttons;
	pthread_t ctrl_loop_id;
	pthread_t heater_ctrl_id;
	volatile int stop_control_loop;
	volatile double heater_duty_cycle;
};

static double query_wrapper(void *p)
{
	return max31865_get_temperature((max31865_t *)p, NULL);
}

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
	struct callback_data *data = (struct callback_data *)user_data;

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
		change_motor_speed(&data->motor, MOTOR_SPEED_DELTA);
		break;
	case BUTTON_4_PIN:
		change_motor_speed(&data->motor, -MOTOR_SPEED_DELTA);
		break;
		break;
	default:
		/* invalid button pin ID */
		assert(0);
	}
}

static void *pidctrl_loop_thread(void *user_data)
{
	struct callback_data *data = (struct callback_data *)user_data;
	const useconds_t num_usecs = 1000 * PID_CONTROL_LOOP_MS;

	while (!data->stop_control_loop) {
		data->heater_duty_cycle = pidctrl_get_output(data->pidctrl);
		usleep(num_usecs);
	}
	return NULL;
}

static uint32_t nearest_multiple(const double value, const uint32_t multiple)
{
	return (ceil(value / multiple) * multiple);
}

static void *heater_control_thread(void *user_data)
{
	struct callback_data *data = (struct callback_data *)user_data;
	uint32_t on_ms, off_ms;
	struct timespec start, end;
	uint32_t n = 0;
	uint32_t total_on = 0;
	uint8_t ssr_state = 0;

	while (!data->stop_control_loop) {
		/* The SSR has a built-in triac, so it will only switch on
		 * zero crossings. Make sure we do not switch the SSR on for
		 * time intervals smaller than a half-period (10ms @ 50Hz,
		 * 8ms @ 60Hz).
		 */
		if (data->heater_duty_cycle >= (10.0 / PID_CONTROL_LOOP_MS)) {
			on_ms = nearest_multiple(
			    PID_CONTROL_LOOP_MS * data->heater_duty_cycle, 10);
			if (on_ms > PID_CONTROL_LOOP_MS) {
				on_ms = PID_CONTROL_LOOP_MS;
			}
			off_ms = PID_CONTROL_LOOP_MS - on_ms;
		} else {
			on_ms = 0;
			off_ms = PID_CONTROL_LOOP_MS;
		}

		total_on += on_ms;
		if (on_ms) {
			/* No need to write to GPIO if SSR is already on */
			if (!ssr_state) {
				clock_gettime(CLOCK_MONOTONIC, &start);
				bcm2835_gpio_write(SSR_PIN, HIGH);
				ssr_state = 1;
				clock_gettime(CLOCK_MONOTONIC, &end);

				on_ms -= (end.tv_sec - start.tv_sec) * 1E3 +
					 (end.tv_nsec - start.tv_nsec) / 1E6;
			}

			usleep(1000 * on_ms);
		}

		if (off_ms) {
			/* No need to write to GPIO if SSR is already off */
			if (ssr_state) {
				clock_gettime(CLOCK_MONOTONIC, &start);
				bcm2835_gpio_write(SSR_PIN, LOW);
				ssr_state = 0;
				clock_gettime(CLOCK_MONOTONIC, &end);

				off_ms -= (end.tv_sec - start.tv_sec) * 1E3 +
					  (end.tv_nsec - start.tv_nsec) / 1E6;
			}

			usleep(1000 * off_ms);
		}

		if (++n % PID_CONTROL_LOOP_HZ == 0) {
			printf("Heater was on for %u ms (%f %%), T = %f \xB0""C\n",
			       total_on, (total_on / 10.0),
			       max31865_get_temperature(&data->maxim, NULL));
			total_on = 0;
		}
	}

	/* make sure SSR is off after the thread finishes */
	bcm2835_gpio_write(SSR_PIN, LOW);

	return NULL;
}

static void configure_SSR_output()
{
	bcm2835_gpio_fsel(SSR_PIN, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(SSR_PIN, LOW);
}

static void cleanup_SSR_output()
{
	bcm2835_gpio_write(SSR_PIN, LOW);
	bcm2835_gpio_fsel(SSR_PIN, BCM2835_GPIO_FSEL_INPT);
}

int main(int argc, char **argv)
{
	/************************************************************************
	 * TODO: read RTD table initalizer values and MAX31865 config parameters
	 *       from config file (libconfig?)
	 ***********************************************************************/

	int status = 0;
	struct callback_data data;
	memset(&data, 0, sizeof(data));

	if (!bcm2835_init()) {
		fprintf(stderr, "Failed to initialize bcm2835 library.\n");
		goto out;
	}
	++status;

	printf("Initializing RTD table\n");
	if (rtd_table_init(0.0, 100.0, 1000.0, 3600.0) == -1) {
		goto out;
	}
	++status;

	max31865_init(&data.maxim, BCM2835_SPI_CS0, MAX31865_DRDY_PIN,
		      MAX31865_4WIRE_RTD);

	motor_init(&data.motor, MOTOR_CLOCK_DIVIDER, MOTOR_PWM_RANGE);
	motor_start(&data.motor);
	motor_set_duty_cycle(&data.motor, MOTOR_PWM_RANGE / 2);

	++status;

	data.pidctrl = pidctrl_init(
	    40.0, 0.5, 25.0, 4.0, &query_wrapper, (void *)&data.maxim,
	    PID_CONTROL_LOOP_MS, PID_MIN_DUTY_CYCLE, PID_MAX_DUTY_CYCLE);
	if (!data.pidctrl) {
		fprintf(stderr, "Failed to initialize PID controller.\n");
		goto out;
	}
	++status;

	data.buttons =
	    buttons_init(BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN, BUTTON_4_PIN,
			 50, &button_callback_handler, (void *)&data);
	if (!data.buttons) {
		fprintf(stderr, "Failed to initialize button handler.\n");
		goto out;
	}
	configure_SSR_output();
	++status;

	if (pthread_create(&data.ctrl_loop_id, NULL,
			   &pidctrl_loop_thread, (void *)&data) == -1) {
		fprintf(stderr, "Failed to create PID control loop thread\n");
		goto out;
	}
	++status;

	/* wait for the PID loop to run a couple of cycles */
	sleep(1);

	if (pthread_create(&data.heater_ctrl_id, NULL,
			   &heater_control_thread, (void *)&data) == -1) {
		fprintf(stderr, "Failed to create heater control thread\n");
		data.stop_control_loop = 1;
		pthread_join(data.ctrl_loop_id, NULL);
		goto out;
	}
	++status;

out:
	switch (status) {
	case 7:
		data.stop_control_loop = 1;
		pthread_join(data.ctrl_loop_id, NULL);
	case 6:
		data.stop_control_loop = 1;
		pthread_join(data.heater_ctrl_id, NULL);
	case 5:
		cleanup_SSR_output();
		buttons_cleanup(data.buttons);
	case 4:
		pidctrl_free(data.pidctrl);
	case 3:
		motor_cleanup(&data.motor);
		max31865_cleanup(&data.maxim);
	case 2:
		rtd_table_free();
	case 1:
		bcm2835_close();
	}

	if (status != 7) {
		exit(EXIT_FAILURE);
	}
	return 0;
}
