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

#ifndef SOUSVIDED_MAX31865
#define SOUSVIDED_MAX31865

#include <stdint.h>
#include <time.h>

enum MAX31865_REGISTER {
	MAX31865_REGISTER_CONFIG = 0x00,
	MAX31865_REGISTER_RTD_MSB = 0x01,
	MAX31865_REGISTER_RTD_LSB = 0x02,
	MAX31865_REGISTER_FAULT_HT_MSB = 0x03,
	MAX31865_REGISTER_FAULT_HT_LSB = 0x04,
	MAX31865_REGISTER_FAULT_LT_MSB = 0x05,
	MAX31865_REGISTER_FAULT_LT_LSB = 0x06,
	MAX31865_REGISTER_FAULT_STATUS = 0x07,
	MAX31865_REGISTER_MAX
};

enum MAX31865_VBIAS {
	MAX31865_VBIAS_OFF = 0,
	MAX31865_VBIAS_ON = 1
};

enum MAX31865_CONVERSION_MODE {
	MAX31865_CONV_MODE_NORMALLY_OFF = 0,
	MAX31865_CONV_MODE_AUTO
};

#define MAX31865_ONE_SHOT_OFF 0
#define MAX31865_ONE_SHOT_ON 1

enum MAX31865_RTD_TYPE {
	MAX31865_2WIRE_RTD = 0,
	MAX31865_4WIRE_RTD = 0,
	MAX31865_3WIRE_RTD = 1
};

enum MAX31865_NOISE_FILTER_HZ {
	MAX31865_NOISE_FILTER_50HZ = 0,
	MAX31865_NOISE_FILTER_60HZ = 1
};

#define MAX31865_FAULT_STATUS_NO_CLEAR 0
#define MAX31865_FAULT_STATUS_AUTO_CLEAR 1

struct max31865
{
	uint8_t initialized;
	uint8_t query_mode;
	uint8_t rtd_type;
	uint8_t config;
	uint8_t cs_pin;
	uint8_t drdy_pin;
	uint16_t rtd;
	uint16_t fault_ht;
	uint16_t fault_lt;
	struct timespec last_query;
};
typedef struct max31865 max31865_t;

int max31865_init(max31865_t *m, const uint8_t cs_pin, const uint8_t drdy_pin,
		  const uint8_t rtd_type);
void max31865_cleanup(max31865_t *m);

uint8_t max31865_get_configuration(max31865_t *m);
uint8_t max31865_read_configuration(max31865_t *m);
int max31865_set_configuration(max31865_t *m, enum MAX31865_VBIAS vbias,
			       enum MAX31865_CONVERSION_MODE conv_mode,
			       int one_shot, enum MAX31865_RTD_TYPE rtd_type,
			       const int fault_detection_ctrl,
			       const int fault_status_clear,
			       enum MAX31865_NOISE_FILTER_HZ noise_filter_hz);

uint16_t max31865_read_rtd(max31865_t *m, uint8_t *fault);
float max31865_get_temperature(max31865_t *m, uint8_t *fault);
float max31865_convert_rtd_to_temperature(const uint16_t rtd);

void max31865_get_fault_thresholds(max31865_t *m, uint16_t *high,
				   uint16_t *low);
void max31865_set_fault_thresholds(max31865_t *m, const uint16_t high,
				   const uint16_t low);

void max31865_set_fault_high_threshold(max31865_t *m, const uint16_t high);
void max31865_set_fault_low_threshold(max31865_t *m, const uint16_t low);

uint8_t max31865_get_fault_status(max31865_t *m);

#endif /* SOUSVIDED_MAX31865 */
