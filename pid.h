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

#ifndef SOUSVIDED_PID_H
#define SOUSVIDED_PID_H

#include <stdint.h>
#include <time.h>

typedef double (*pidctrl_query_fn)(void *);

struct pidctrl
{
	double set_point;

	double kp;
	double ki;
	double kd;

	double integral;
	double last_input;
	double output;
	double output_min;
	double output_max;

	pidctrl_query_fn query_fn;
	void *user_data;

	struct timespec last_query;
	uint32_t delta_t;
};

typedef struct pidctrl pidctrl_t;

pidctrl_t *pidctrl_init(const double sp, const double kp, const double ki,
			const double kd, pidctrl_query_fn query_fn,
			void *user_data, const uint32_t delta_t_ms,
			const double output_min, const double output_max);
void pidctrl_free(pidctrl_t *p);

void pidctrl_set_delta_t(pidctrl_t *p, const uint32_t delta_t_ms);

void pidctrl_set_set_point(pidctrl_t *p, const double sp);
void pidctrl_tune(pidctrl_t *p, const double kp, const double ki,
		  const double kd);

void pidctrl_set_limits(pidctrl_t *p, const double min, const double max);
void pidctrl_get_limits(const pidctrl_t *p, double *min, double *max);

void pidctrl_set_query_callback(pidctrl_t *p, pidctrl_query_fn query_fn,
				void *user_data);

void pidctrl_set_user_data(pidctrl_t *p, void *user_data);

double pidctrl_get_output(pidctrl_t *p);

#endif /* SOUSVIDED_PID_H */
