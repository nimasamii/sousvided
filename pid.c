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

#include "pid.h"

#include <assert.h>
#include <stdlib.h>

static double clamp(const double v, const double min, const double max)
{
	if (v < min) {
		/* Our PID controller will control the duty cyle of a
		 * heating element switched by a solid state relay.
		 * The relay has a triac and therefore only switches
		 * on/off zero-crossings.
		 * The min limit should therefore be the minimal time the
		 * heating element can be switched on (20ms @ 50Hz or
		 * ~16ms @ 60Hz mains frequency) and not be a lower limit
		 * but the minimal on-time threshold
		 */
		return 0.0;
	} else if (v > max) {
		return max;
	}

	return v;
}

static uint32_t delta_t_ms(const struct timespec *start,
			   const struct timespec *end)
{
	assert(start != NULL);
	assert(end != NULL);

	return (end->tv_sec - start->tv_sec) * 1000 +
	       (end->tv_nsec - start->tv_sec) / 1000000;
}

static void update_output(pidctrl_t *p)
{
	const double input = p->query_fn(p->user_data);
	const double error = p->set_point - input;
	const double delta_input = input - p->last_input;

	p->integral =
	    clamp(p->integral + (p->ki * error), p->output_min, p->output_max);
	p->last_input = input;
	p->output = clamp(p->kp * error + p->integral - (p->ki * delta_input),
			  p->output_min, p->output_max);
}

pidctrl_t *pidctrl_init(const double sp, const double kp, const double ki,
			const double kd, pidctrl_query_fn query_fn,
			void *user_data, const uint32_t delta_t_ms,
			const double output_min, const double output_max)
{
	assert(query_fn != NULL);
	assert(delta_t_ms > 0);

	pidctrl_t *p = (pidctrl_t *)malloc(sizeof(pidctrl_t));
	if (p) {
		p->set_point = sp;

		p->kp = kp;
		p->ki = ki * (delta_t_ms * 0.001);
		p->kd = kd / (delta_t_ms * 0.001);

		p->integral = 0.0;
		p->output = 0.0;
		p->output_min = output_min;
		p->output_max = output_max;

		p->query_fn = query_fn;
		p->user_data = user_data;

		p->delta_t = delta_t_ms;

		p->last_input = query_fn(user_data);
		clock_gettime(CLOCK_MONOTONIC, &p->last_query);
	}
	return p;
}

void pidctrl_free(pidctrl_t *p)
{
	free(p);
}

void pidctrl_set_delta_t(pidctrl_t *p, const uint32_t delta_t_ms)
{
	assert(p != NULL);
	p->delta_t = delta_t_ms;
}

double pidctrl_get_set_point(pidctrl_t *p)
{
	assert(p != NULL);
	return p->set_point;
}

void pidctrl_set_set_point(pidctrl_t *p, const double sp)
{
	assert(p != NULL);
	assert(sp >= 0.0);
	p->set_point = sp;
}

void pidctrl_tune(pidctrl_t *p, const double kp, const double ki,
		  const double kd)
{
	assert(p != NULL);

	p->kp = kp;
	p->ki = ki * (p->delta_t * 1.0E-3);
	p->kd = kd / (p->delta_t * 1.0E-31);
}

void pidctrl_set_limits(pidctrl_t *p, const double min, const double max)
{
	assert(p != NULL);

	p->output_min = min;
	p->output_max = max;
	p->integral = clamp(p->integral, min, max);
	p->output = clamp(p->output, min, max);
}

void pidctrl_get_limits(const pidctrl_t *p, double *min, double *max)
{
	assert(p != NULL);

	if (min) {
		*min = p->output_min;
	}

	if (max) {
		*max = p->output_max;
	}
}

void pidctrl_set_query_callback(pidctrl_t *p, pidctrl_query_fn query_fn,
				void *user_data)
{
	assert(p != NULL);
	assert(query_fn != NULL);

	p->query_fn = query_fn;
	p->user_data = user_data;
}

void pidctrl_set_user_data(pidctrl_t *p, void *user_data)
{
	assert(p != NULL);
	p->user_data = user_data;
}

double pidctrl_get_output(pidctrl_t *p)
{
	assert(p != NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (delta_t_ms(&p->last_query, &now) >= p->delta_t) {
		p->last_query = now;
		update_output(p);
	}

	return p->output;
}
