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
#include <string.h>

struct rtd_table
{
	float *data;
	unsigned int base;
	unsigned int size;
	float min_temp;
	float max_temp;
};

struct rtd_table RTD_TABLE = { NULL, 0, 32768, 0.0f, 100.0f };

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
#ifdef FP_FAST_FMA
	/* Use hardware fused multiply-add if possible */
	return fma(fma(CVD_B, T, CVD_A), T, 1.0);
#else
	return ((CVD_B * T + CVD_A) * T + 1.0);
#endif /* FP_FAST_FMA */
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
#ifdef FP_FAST_FMA
	/* Use hardware fused multiply-add if possible */
	return fma(2.0 * CVD_B, T, CVD_A);
#else
	return CVD_A + (2.0 * CVD_B * T);
#endif /* FP_FAST_FMA */
}

/* approximate the temperature for a given resistance using Newton's method */
double newton_approx(const double R, const unsigned int R0,
		     const double max_residual)
{
	double T = 0.0;
	double residual;
	do {
		residual = R - R0 * callendar_van_dusen(T);
		T += residual / (R0 * callendar_van_dusen_derivative(T));
	} while (fabs(residual) > max_residual);
	return T;
}

static double resistance_from_adc(const unsigned int adc,
				  const unsigned int reference_resistance)
{
	assert(adc < 32768);
	return ((adc * reference_resistance) / 32768.0);
}

static unsigned int find_minimum_adc(const double resistance_min,
				     const unsigned int reference_resistance)
{
	unsigned int adc = 0;
	while (adc < 32768 && (resistance_from_adc(adc, reference_resistance) <
			       resistance_min)) {
		++adc;
	}

	if (adc > 0) {
		--adc;
	}

	return adc;
}

static unsigned int find_maximum_adc(const double resistance_max,
				     const unsigned int reference_resistance,
				     const unsigned int adc_min)
{
	unsigned int adc = 32767;
	while (adc > 0 && (resistance_from_adc(adc, reference_resistance) >
			   resistance_max)) {
		--adc;
	}

	if (!adc) {
		++adc;
	}

	if (adc <= adc_min) {
		adc = adc_min + 1;
	}

	return adc;
}

static int generate_rtd_table(struct rtd_table *table,
			      const double temperature_min,
			      const double temperature_max,
			      const unsigned int R0,
			      const unsigned int reference_resistance)
{
	assert(table != NULL);

	if (R0 < 100) {
		fprintf(stderr, "generate_rtd_table: R0 too small (%d < 100)\n",
			R0);
		return -1;
	}

	if (temperature_min >= temperature_max) {
		fprintf(stderr, "generate_rtd_table: invalid temperature "
				"boundaries (%f > %f)\n",
			temperature_min, temperature_max);
		return -1;
	}

	const double resistance_min = R0 * callendar_van_dusen(temperature_min);
	const double resistance_max = R0 * callendar_van_dusen(temperature_max);

	const unsigned int adc_min =
	    find_minimum_adc(resistance_min, reference_resistance);
	const unsigned int adc_max =
	    find_maximum_adc(resistance_max, reference_resistance, adc_min);

	table->data = malloc(sizeof(float) * (adc_max - adc_min + 1));
	if (!table->data) {
		fprintf(stderr,
			"generate_rtd_table: failed to allocate table.\n");
		return -1;
	}
	printf("generate_rtd_table: allocated %zu bytes for RTD table\n",
	       sizeof(float) * (adc_max - adc_min + 1));

	unsigned int adc;
	for (adc = adc_min; adc < adc_max; ++adc) {
		double resistance =
		    resistance_from_adc(adc, reference_resistance);
		table->data[adc - adc_min] =
		    newton_approx(resistance, R0, 1.0e-6);
	}

	table->base = adc_min;
	table->size = adc_max - adc_min + 1;
	table->min_temp = temperature_min;
	table->max_temp = temperature_max;

	return 0;
}

int rtd_table_init(const double temperature_min, const double temperature_max,
		   const int R0, const int reference_resistance)
{
	assert(RTD_TABLE.data == NULL);

	struct rtd_table table;
	if (generate_rtd_table(&table, temperature_min, temperature_max, R0,
			       reference_resistance) == -1) {
		fprintf(stderr, "failed to initialize RTD table\n");
		return -1;
	}
	memcpy(&RTD_TABLE, &table, sizeof(table));
	return 0;
}

void rtd_table_free()
{
	free(RTD_TABLE.data);
}

float rtd_table_query(const unsigned int adc)
{
	assert(adc < 32768);
	assert(RTD_TABLE.data != NULL);

	if (adc < RTD_TABLE.base) {
		return RTD_TABLE.min_temp;
	} else if ((adc - RTD_TABLE.base) >= RTD_TABLE.size) {
		return RTD_TABLE.max_temp;
	}
	return RTD_TABLE.data[adc - RTD_TABLE.base];
}
