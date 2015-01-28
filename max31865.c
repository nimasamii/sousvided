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

#include "max31865.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include "bcm2835.h"
#include "rtd_table.h"

static uint8_t read_register8(const max31865_t *m,
			      const enum MAX31865_REGISTER reg)
{
	assert(m != NULL);
	assert(reg != MAX31865_REGISTER_MAX);

	uint16_t data = ((uint16_t)reg << 8) & 0x7F00;
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, LOW);
	bcm2835_spi_transfern((char *)&data, sizeof(data));
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, HIGH);
	return (uint8_t)(data & 0x00FF);
}

static uint16_t read_register16(const max31865_t *m,
				const enum MAX31865_REGISTER reg)
{
	assert(m != NULL);
	assert(reg != MAX31865_REGISTER_MAX);

	uint8_t data[3] = { reg & 0x7F, 0, 0 };
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, LOW);
	bcm2835_spi_transfern((char *)&data, sizeof(data));
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, HIGH);
	return (uint16_t)data[1] << 8 | (uint16_t)data[2];
}

static uint32_t read_register32(const max31865_t *m,
				const enum MAX31865_REGISTER reg)
{
	assert(m != NULL);
	assert(reg != MAX31865_REGISTER_MAX);

	uint8_t data[5] = { reg & 0x7F, 0, 0, 0, 0 };
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, LOW);
	bcm2835_spi_transfern((char *)&data, sizeof(data));
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, HIGH);
	return ((uint32_t)data[1] << 24 | (uint32_t)data[2] << 16 |
		(uint32_t)data[3] << 8 | (uint32_t)data[4]);
}

static void write_register8(const max31865_t *m,
			    const enum MAX31865_REGISTER reg,
			    const uint8_t value)
{
	assert(m != NULL);
	assert(reg != MAX31865_REGISTER_MAX);

	uint16_t data = (((uint16_t)reg << 8) & 0xFF00) | 0x8000 |
			((uint16_t)value & 0x00FF);
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, LOW);
	bcm2835_spi_transfern((char *)&data, sizeof(data));
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, HIGH);
}

static void write_register16(const max31865_t *m,
			     const enum MAX31865_REGISTER reg,
			     const uint16_t value)
{
	assert(m != NULL);
	assert(reg != MAX31865_REGISTER_MAX);

	uint8_t data[3] = {(uint8_t)reg | 0x8000, (value & 0xFF00) >> 8,
			   value & 0x00FF };
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, LOW);
	bcm2835_spi_transfern((char *)&data, sizeof(data));
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, HIGH);
}

static void write_register32(const max31865_t *m,
			     const enum MAX31865_REGISTER reg,
			     const uint32_t value)
{
	assert(m != NULL);
	assert(reg != MAX31865_REGISTER_MAX);

	uint8_t data[5] = {(uint8_t)reg | 0x8000,
			   (value & 0xFF000000) >> 24,
			   (value & 0x00FF0000) >> 16,
			   (value & 0x0000FF00) >> 8,
			   value & 0x000000FF };
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, LOW);
	bcm2835_spi_transfern((char *)&data, sizeof(data));
	bcm2835_spi_setChipSelectPolarity(m->cs_pin, HIGH);
}

int max31865_init(max31865_t *m, const uint8_t cs_pin, const uint8_t drdy_pin,
		  const uint8_t rtd_type)
{
	assert(m != NULL);
	assert(!m->initialized);

	m->rtd = 0;
	m->cs_pin = cs_pin;
	m->drdy_pin = drdy_pin;
	m->rtd_type = rtd_type;

	/* initialize GPIO pins for SPI operations */
	bcm2835_spi_begin();

	/* set SPI bit order to most significant bit first.
	 * NOTE: This is the only modethe BCM2835 chip supports, fortunately so
	 *       does the MAX31865.
	 */
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);

	/* set SPI to operate in mode 1 (clock polarity = 0, clock phase = 1) */
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);

	/* set SPI clock frequency to 5 MHz (base clock is 250 MHz by default)
	 */
	bcm2835_spi_setClockDivider(50);

	/* set SPI CE polarity to be active low */
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

	/* set SPI chip select pin (CE0|CE1) to be asserted on data transfer */
	bcm2835_spi_chipSelect(cs_pin);

	/* configure input detection on DRDY pin:
	 *   - set drdy_pin as input
	 *   - enable pull up resistor
	 *   - enable low detect enable
	 */
	bcm2835_gpio_fsel(drdy_pin, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(drdy_pin, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_len(drdy_pin);

	m->initialized = 1;

	/* read initial fault thresholds */
	max31865_get_fault_thresholds(m, NULL, NULL);

	int rc = max31865_set_configuration(
	    m, MAX31865_VBIAS_ON, MAX31865_CONVMODE_AUTO, MAX31865_ONE_SHOT_OFF,
	    rtd_type, 0, MAX31865_FAULT_STATUS_AUTO_CLEAR,
	    MAX31865_50HZ_FILTER);
	assert(rc == 0);
	return rc;
}

void max31865_cleanup(max31865_t *m)
{
	assert(m != NULL);
	assert(m->initialized);

	/* turn off automatic conversion */
	max31865_set_configuration(m, MAX31865_VBIAS_OFF, MAX31865_CONVMODE_OFF,
				   MAX31865_ONE_SHOT_OFF, m->rtd_type, 0,
				   MAX31865_FAULT_STATUS_AUTO_CLEAR,
				   MAX31865_50HZ_FILTER);

	/* disable low detect enable */
	bcm2835_gpio_clr_len(m->drdy_pin);

	/* clear EDS flag */
	bcm2835_gpio_set_eds(m->drdy_pin);

	/* enable pull down resistor */
	bcm2835_gpio_set_pud(m->drdy_pin, BCM2835_GPIO_PUD_DOWN);

	/* return the GPIO SPI pins to their default setting */
	bcm2835_spi_end();

	m->initialized = 0;
}

uint8_t max31865_get_configuration(max31865_t *m)
{
	assert(m != NULL);
	assert(m->initialized);
	return m->config;
}

uint8_t max31865_read_configuration(max31865_t *m)
{
	assert(m != NULL);
	assert(m->initialized);

	uint8_t config = read_register8(m, MAX31865_REGISTER_CONFIG);
	assert(config == m->config);
	return config;
}

int max31865_set_configuration(max31865_t *m, const int vbias,
			       const int conv_mode, const int one_shot,
			       const int rtd_type,
			       const int fault_detection_ctrl,
			       const int fault_status_clear,
			       const int noise_filter_frequency)
{
	assert(m != NULL);
	assert(m->initialized);

	/* Config register:
	 * D7     D6   D5     D4     D3     D2     D1    D0
	 * VBias  Conv 1-shot 3-wire Fault  Fault  Fault 50/60Hz
	 *        Mode        RTD    CycleH CycleL Status Filter
	 * 0x80   0x40 0x20   0x10   0x08   0x04   0x02   0x01
	 */
	uint8_t config = noise_filter_frequency & 0x01;
	if (fault_status_clear == MAX31865_FAULT_STATUS_AUTO_CLEAR) {
		config |= 0x02;
	}

	if (fault_detection_ctrl) {
		config |= ((fault_detection_ctrl & 0x03) << 3);
	}

	if (rtd_type == MAX31865_3WIRE_RTD) {
		config |= 0x10;
	}

	if (one_shot) {
		if (conv_mode != MAX31865_CONVMODE_OFF) {
			errno = EINVAL;
			return -1;
		}
		config |= 0x20;
	}

	if (conv_mode) {
		if (one_shot != MAX31865_ONE_SHOT_OFF) {
			errno = EINVAL;
			return -1;
		}
		config |= 0x40;
	}

	if (vbias == MAX31865_VBIAS_ON) {
		config |= 0x80;
	}

	write_register8(m, MAX31865_REGISTER_CONFIG, config);

	/* Don't save fault status auto-clear bit as the MAX31865 sets it to 0
	 * after clearing the fault status register */
	m->config = config & ~(0x02);

	return 0;
}

uint16_t max31865_read_rtd(max31865_t *m, uint8_t *fault)
{
	assert(m != NULL);
	assert(m->initialized);

	/* check if DRDY signaled new temperature readout */
	if (bcm2835_gpio_eds(m->drdy_pin)) {
		/* readout new resistance value and clear EDS flag */
		m->rtd = read_register16(m, MAX31865_REGISTER_RTD_MSB);
		if (fault) {
			*fault = m->rtd & 0x0001;
		}
		m->rtd >>= 1;

		bcm2835_gpio_set_eds(m->drdy_pin);
	}
	return (m->rtd & 0x7FFF);
}

float max31865_get_temperature(max31865_t *m, uint8_t *fault)
{
	assert(m != NULL);
	assert(m->initialized);
	assert(RTD_TABLE != NULL);

	uint16_t rtd = max31865_read_rtd(m, fault);
	return RTD_TABLE[rtd];
}

float max31865_convert_rtd_to_temperature(const uint16_t rtd)
{
	assert(RTD_TABLE != NULL);
	assert((rtd & 0x8000) == 0);
	return RTD_TABLE[rtd & 0x7FFF];
}

void max31865_get_fault_thresholds(max31865_t *m, uint16_t *high, uint16_t *low)
{
	assert(m != NULL);
	assert(m->initialized);

	uint32_t value = read_register32(m, MAX31865_REGISTER_FAULT_HT_MSB);
	m->fault_ht = (value & 0xFFFF0000) >> 16;
	m->fault_lt = (value & 0x0000FFFF);

	if (high && high != &m->fault_ht) {
		*high = m->fault_ht;
	}

	if (low && low != &m->fault_lt) {
		*low = m->fault_lt;
	}
}

void max31865_set_fault_thresholds(max31865_t *m, const uint16_t high,
				   const uint16_t low)
{
	assert(m != NULL);
	assert(m->initialized);

	uint32_t value = ((uint32_t)high << 16) | (uint32_t)low;
	write_register32(m, MAX31865_REGISTER_FAULT_HT_MSB, value);
	m->fault_ht = high;
	m->fault_lt = low;
}

void max31865_set_fault_high_threshold(max31865_t *m, const uint16_t high)
{
	assert(m != NULL);
	assert(m->initialized);

	write_register16(m, MAX31865_REGISTER_FAULT_HT_MSB, high);
	m->fault_ht = high;
}

void max31865_set_fault_low_threshold(max31865_t *m, const uint16_t low)
{
	assert(m != NULL);
	assert(m->initialized);

	write_register16(m, MAX31865_REGISTER_FAULT_LT_MSB, low);
	m->fault_lt = low;
}

uint8_t max31865_get_fault_status(max31865_t *m)
{
	assert(m != NULL);
	assert(m->initialized);

	return read_register8(m, MAX31865_REGISTER_FAULT_STATUS);
}
