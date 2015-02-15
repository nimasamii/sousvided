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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

float *RTD_TABLE = NULL;

static const double CVD_A = 3.9083E-3;
static const double CVD_B = -5.775E-7;
static const double CVD_C = -4.18301E-12;

/* Calculate the normalized resistance at temperature T (in degrees Celsius)
 * using the Callendar Van Dusen Equation */
static double callendar_van_dusen(const double T)
{
	/* R(T) = 1 + AT + BT^2                 (T >= 0)
	 *        1 + AT + BT^2 + C(T - 100)T^3	(T < 0)
	 */
	if (T < 0.0) {
		return (((CVD_C * (T - 100.0) * T - CVD_B) * T + CVD_A) * T +
			1.0);
	}
	return ((CVD_B * T + CVD_A) * T + 1.0);
}

/* Calculate the derivative of the normalized resistance at temperature T (in
 * degrees Celsius), using the Callendar Van Dusen Equation */
static double callendar_van_dusen_derivative(const double T)
{
	/* dR/dt = A + 2BT                            (T >= 0.0)
	 *         A + 2BT + C * (-300 T^2 + 4 T ^3)  (T < 0.0)
	 */

	if (T < 0.0) {
		return ((4.0 * T - 300.0) * CVD_C * T - 2.0 * CVD_B) * T +
		       CVD_A;
	}
	return CVD_A + (2.0 * CVD_B * T);
}

/* approximate the temperature for a given resistance using Newton's method */
double newton_approx(const double R, const int R0, const double max_residual)
{
	double T = 0.0;
	double residual;
	do {
		residual = R - R0 * callendar_van_dusen(T);
		T += residual / (R0 * callendar_van_dusen_derivative(T));
	} while (fabs(residual) > max_residual);
	return T;
}

static double resistance_from_adc(const int adc, const int reference_resistance)
{
	assert(adc < 32768);
	return ((adc * reference_resistance) / 32768.0);
}

static float *generate_rtd_table(const double temperature_min,
				 const double temperature_max, const int R0,
				 const int reference_resistance)
{
	if (R0 < 100) {
		fprintf(stderr, "generate_rtd_table: R0 too small (%d < 100)\n",
			R0);
		return NULL;
	}

	if (temperature_min >= temperature_max) {
		fprintf(stderr, "generate_rtd_table: invalid temperature "
				"boundaries (%f > %f)\n",
			temperature_min, temperature_max);
		return NULL;
	}

	float *rtd_table = (float *)malloc(sizeof(float) * 32768);
	if (!rtd_table) {
		fprintf(stderr,
			"generate_rtd_table: failed to allocate table.\n");
		return NULL;
	}

	const double resistance_min = R0 * callendar_van_dusen(temperature_min);
	const double resistance_max = R0 * callendar_van_dusen(temperature_max);

	double resistance;
	int adc;
	for (adc = 0; adc < 32768; ++adc) {
		resistance = resistance_from_adc(adc, reference_resistance);
		if (resistance < resistance_min) {
			rtd_table[adc] = temperature_min;
		} else if (resistance > resistance_max) {
			rtd_table[adc] = temperature_max;
		} else {
			rtd_table[adc] = newton_approx(resistance, R0, 1.0e-6);
		}
	}
	return rtd_table;
}

int rtd_table_init(const double temperature_min, const double temperature_max,
		   const int R0, const int reference_resistance)
{
	assert(RTD_TABLE == NULL);

	RTD_TABLE = generate_rtd_table(temperature_min, temperature_max, R0,
				       reference_resistance);
	if (!RTD_TABLE) {
		fprintf(stderr, "failed to initialize RTD table\n");
		return -1;
	}
	return 0;
}

void rtd_table_free()
{
	free(RTD_TABLE);
	RTD_TABLE = NULL;
}
