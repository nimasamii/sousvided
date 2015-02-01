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

#include "buttons.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>

#include "bcm2835.h"

void button_init(button_t *btn, const uint8_t pin)
{
	assert(btn != NULL);
	assert(!btn->initialized);

	btn->pin = pin;
	btn->last_event = 0;

	/* set pin as input */
	bcm2835_gpio_fsel(btn->pin, BCM2835_GPIO_FSEL_INPT);

	/* activate rising edge detection */
	bcm2835_gpio_ren(btn->pin);

	btn->initialized = 1;
}

void button_cleanup(button_t *btn)
{
	assert(btn != NULL);
	assert(btn->initialized);

	/* disable rising edge detection */
	bcm2835_gpio_clr_ren(btn->pin);

	/* clear event detection status for this pin */
	bcm2835_gpio_set_eds(btn->pin);

	btn->pin = 0xFF;
	btn->last_event = 0xFFFFFFFF;
	btn->initialized = 0;
}

static int notify_needed(button_t *btn, const uint32_t debounce,
			 const uint64_t milli_secs)
{
	assert(btn != NULL);

	if (bcm2835_gpio_eds(btn->pin)) {
		bcm2835_gpio_set_eds(btn->pin);
		if (milli_secs - btn->last_event > debounce) {
			btn->last_event = milli_secs;
			return 1;
		}
	}
	return 0;
}

static void *button_ctrl_thread(void *user_data)
{
	assert(user_data != NULL);
	buttons_t *btns = (buttons_t *)user_data;

	assert(btns->initialized);

	struct timespec now;
	uint64_t milli_secs;

	while (1) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		milli_secs = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);

		if (notify_needed(&btns->incr_temperature, milli_secs,
				  btns->debounce)) {
			btns->callback(btns->incr_temperature.pin,
				       btns->user_data);
		}

		if (notify_needed(&btns->decr_temperature, milli_secs,
				  btns->debounce)) {
			btns->callback(btns->decr_temperature.pin,
				       btns->user_data);
		}

		if (notify_needed(&btns->incr_motor_speed, milli_secs,
				  btns->debounce)) {
			btns->callback(btns->incr_motor_speed.pin,
				       btns->user_data);
		}

		if (notify_needed(&btns->decr_motor_speed, milli_secs,
				  btns->debounce)) {
			btns->callback(btns->decr_motor_speed.pin,
				       btns->user_data);
		}

		if (btns->stop_thread) {
			break;
		}

		usleep(50000);
	}

	return NULL;
}

buttons_t *buttons_init(const uint8_t inc_temp_pin, const uint8_t dec_temp_pin,
			const uint8_t inc_motor_pin,
			const uint8_t dec_motor_pin, const uint32_t debounce,
			button_callback_fn callback, void *user_data)
{
	assert(callback != NULL);

	buttons_t *btns = (buttons_t *)malloc(sizeof(buttons_t));
	if (btns) {
		memset(btns, 0, sizeof(buttons_t));

		btns->debounce = debounce;
		button_init(&btns->incr_temperature, inc_temp_pin);
		button_init(&btns->decr_temperature, dec_temp_pin);
		button_init(&btns->incr_motor_speed, inc_motor_pin);
		button_init(&btns->decr_motor_speed, dec_motor_pin);

		btns->initialized = 1;

		if (pthread_create(&btns->thread_id,
				   NULL, &button_ctrl_thread,
				   (void *)btns) != 0) {
			button_cleanup(&btns->decr_motor_speed);
			button_cleanup(&btns->incr_motor_speed);
			button_cleanup(&btns->decr_temperature);
			button_cleanup(&btns->incr_temperature);
			free(btns);
			btns = NULL;
		}
	}
	return btns;
}

void buttons_cleanup(buttons_t *btns)
{
	assert(btns != NULL);
	assert(btns->initialized);

	/* stop the button control thread */
	btns->stop_thread = 1;
	pthread_join(btns->thread_id, NULL);

	/* cleanup all button settings */
	button_cleanup(&btns->decr_motor_speed);
	button_cleanup(&btns->incr_motor_speed);
	button_cleanup(&btns->decr_temperature);
	button_cleanup(&btns->incr_temperature);

	free(btns);
}
