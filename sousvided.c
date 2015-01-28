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
#include "rtd_table.h"

#define MOTOR_CLOCK_DIVIDER BCM2835_PWM_CLOCK_DIVIDER_1024

int main(int argc, char **argv)
{
	/************************************************************************
	 * TODO: read RTD table initalizer values and MAX31865 config parameters
	 *       from config file (libconfig?)
	 ***********************************************************************/

	max31865_t maxim;
	motor_t motor;

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

	motor_cleanup(&motor);
	max31865_cleanup(&maxim);
	free_rtd_table();
	return 0;
}
