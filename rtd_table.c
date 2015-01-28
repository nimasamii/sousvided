#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const double A = 3.9083E-3;
static const double B = -5.775 - 7;
static const double C = -4.18301E-12;

float *RTD_TABLE = NULL;

static double *make_resistances(const double T0, const double Tmax,
				const double dT, const double R0,
				size_t *num_resistances)
{
	assert(T0 < Tmax);
	assert(dT > 0.0 && dT < Tmax - T0);
	assert(R0 > 0.0);
	assert(num_resistances != NULL);

	double *resistances = NULL;
	size_t i = 0;
	double T = T0;

	*num_resistances = ((Tmax + dT) - T0) / dT + 2;
	resistances = (double *)malloc(*num_resistances * sizeof(double));
	if (!resistances) {
		fprintf(stderr,
			"make_resistances: failed to allocate buffer\n");
		return NULL;
	}

	for (; T < 0.0 && T < (Tmax + dT); ++i, T += dT) {
		resistances[i] = R0 * (1.0 * (A * T) + (B * pow(T, 2)) +
				       (C * (T - 100.0) * pow(T, 3)));
	}

	for (; T < (Tmax + dT); ++i, T += dT) {
		resistances[i] = R0 * (1.0 + (A * T) + (B * pow(T, 2)));
	}
	*num_resistances = i;

	return resistances;
}

static size_t find_nearest_index(const double *resistances, const size_t n,
				 const double r, const size_t start_index)
{
	assert(resistances);
	assert(n > 0);
	size_t i = start_index;

	if (r <= resistances[0]) {
		return resistances[0];
	} else if (r >= resistances[n - 1]) {
		return resistances[n - 1];
	}

	if (i < 1) {
		i = 1;
	}

	while (i < n && r >= resistances[i]) {
		++i;
	}

	assert(i < n);
	return i;
}

static double temperature_from_index(const size_t index, const double T0,
				     const double dT)
{
	return (T0 + index * dT);
}

static double adc_to_resistance(const uint16_t adc, const double r_ref)
{
	assert(adc < 32768);
	return ((adc * r_ref) / 32768.0);
}

static double interpolate_temperature(const double *resistances, const double r,
				      const double T0, const double dT,
				      const size_t index)
{
	assert(resistances);
	const double R1 = resistances[index - 1];
	const double T1 = temperature_from_index(index - 1, T0, dT);
	const double slope = dT / (resistances[index] - resistances[index - 1]);

	return (T1 + slope * (r - R1));
}

static float *generate_rtd_table(const double T0, const double Tmax,
				 const double dT, const double R0,
				 const double r_ref)
{
	size_t num_resistances = 0;
	double *resistances = NULL;
	float *rtd_table = NULL;
	uint16_t i = 0;
	size_t last_index = 0;

	if (R0 < 100.0) {
		fprintf(stderr, "generate_rtd_table: R0 too small (%f < 100)\n",
			R0);
		goto out;
	} else if (r_ref < R0) {
		fprintf(stderr, "generate_rtd_table: reference resistance "
				"smaller than R0 (%f < %f)\n",
			r_ref, R0);
		goto out;
	}

	if (T0 >= Tmax) {
		fprintf(stderr, "generate_rtd_table: invalid temperature "
				"boundaries (%f > %f)\n",
			T0, Tmax);
		goto out;
	} else if (dT >= ((Tmax - T0) / 10.0)) {
		fprintf(stderr,
			"generate_rtd_table: delta T too large (%f > %f)\n", dT,
			((Tmax - T0) / 10.0));
		goto out;
	}

	resistances = make_resistances(T0, Tmax, dT, R0, &num_resistances);
	if (!resistances) {
		fprintf(stderr,
			"generate_rtd_table: failed to generate resistances\n");
		goto out;
	}

	rtd_table = (float *)malloc(32768 * sizeof(float));
	if (!rtd_table) {
		fprintf(stderr,
			"generate_rtd_table: failed to allocate table.\n");
		free(resistances);
		goto out;
	}

	for (i = 0; i < 32768; ++i) {
		const double r = adc_to_resistance(i, r_ref);
		if (r <= resistances[0]) {
			rtd_table[i] = T0;
			continue;
		} else if (r >= resistances[num_resistances - 1]) {
			rtd_table[i] = Tmax;
			continue;
		}

		const size_t index = find_nearest_index(
		    resistances, num_resistances, r, last_index);
		last_index = index;
		rtd_table[i] =
		    interpolate_temperature(resistances, r, T0, dT, index);
	}

	free(resistances);
out:
	return rtd_table;
}

int init_rtd_table(const double T0, const double Tmax, const double dT,
		   const double R0, const double r_ref)
{
	assert(RTD_TABLE == NULL);

	RTD_TABLE = generate_rtd_table(T0, Tmax, dT, R0, r_ref);
	if (!RTD_TABLE) {
		fprintf(stderr, "failed to initialize RTD table\n");
		return -1;
	}
	return 0;
}

void free_rtd_table()
{
	free(RTD_TABLE);
	RTD_TABLE = NULL;
}
