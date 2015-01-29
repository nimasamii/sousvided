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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcm2835.h"
#include "max31865.h"
#include "motor.h"
#include "pid.h"
#include "rtd_table.h"

#define MOTOR_CLOCK_DIVIDER BCM2835_PWM_CLOCK_DIVIDER_1024

#define PID_UPDATE_FREQUENCY_MS 1000
#define PID_MIN_DUTY_CYCLE 0.02
#define PID_MAX_DUTY_CYCLE 1.0

static double query_wrapper(void *p)
{
	return max31865_get_temperature((max31865_t *)p, NULL);
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

	if (!bcm2835_init()) {
		fprintf(stderr, "Failed to initialize bcm2835 library.\n");
		exit(EXIT_FAILURE);
	}

	printf("Initializing RTD table\n");
	if (init_rtd_table(0.0, 200.0, 0.1, 1000.0, 3600.0) == -1) {
		exit(EXIT_FAILURE);
	}

	memset(&maxim, 0, sizeof(maxim));
	max31865_init(&maxim, BCM2835_SPI_CS0, RPI_V2_GPIO_P1_22,
		      MAX31865_4WIRE_RTD);

	memset(&motor, 0, sizeof(motor));
	motor_init(&motor, MOTOR_CLOCK_DIVIDER, 1000);

	pidctrl = pidctrl_init(40.0, 0.5, 25.0, 4.0, &query_wrapper, &maxim,
			       PID_UPDATE_FREQUENCY_MS, PID_MIN_DUTY_CYCLE,
			       PID_MAX_DUTY_CYCLE);
	if (!pidctrl) {
		fprintf(stderr, "Failed to initialize PID controller.\n");
		motor_cleanup(&motor);
		max31865_cleanup(&maxim);
		free_rtd_table();
		exit(EXIT_FAILURE);
	}

	pidctrl_free(pidctrl);
	motor_cleanup(&motor);
	max31865_cleanup(&maxim);
	free_rtd_table();
	return 0;
}
