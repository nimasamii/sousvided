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

#ifndef SOUSVIDED_BUTTONS_H
#define SOUSVIDED_BUTTONS_H

#include <stdint.h>

#include <sys/types.h> /* for pthread_t */

typedef void (*button_callback_fn)(const uint8_t, void *);

struct button
{
	uint8_t initialized;
	uint8_t pin;
	uint32_t last_event;
};

typedef struct button button_t;

void button_init(button_t *btn, const uint8_t pin);
void button_cleanup(button_t *btn);

struct buttons
{
	volatile uint8_t stop_thread;
	uint8_t initialized;
	uint32_t debounce;

	button_t incr_temperature;
	button_t decr_temperature;
	button_t incr_motor_speed;
	button_t decr_motor_speed;

	button_callback_fn callback;
	void *user_data;

	pthread_t thread_id;
};

typedef struct buttons buttons_t;

buttons_t *buttons_init(const uint8_t inc_temp_pin, const uint8_t dec_temp_pin,
			const uint8_t inc_motor_pin, const uint8_t dec_motor_pin,
			const uint32_t debounce, button_callback_fn callback,
			void *user_data);
void buttons_cleanup(buttons_t *btns);

#endif /* SOUSVIDED_BUTTONS_H */
