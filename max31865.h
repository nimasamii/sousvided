#ifndef SOUSVIDED_MAX31865
#define SOUSVIDED_MAX31865

#include <stdint.h>

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

#define MAX31865_VBIAS_OFF 0
#define MAX31865_VBIAS_ON 1

#define MAX31865_CONVMODE_OFF 0
#define MAX31865_CONVMODE_AUTO 1

#define MAX31865_ONE_SHOT_OFF 0
#define MAX31865_ONE_SHOT_ON 1

#define MAX31865_2WIRE_RTD 0
#define MAX31865_4WIRE_RTD 0
#define MAX31865_3WIRE_RTD 1

#define MAX31865_50HZ_FILTER 0
#define MAX31865_60HZ_FILTER 1

#define MAX31865_FAULT_STATUS_NO_CLEAR 0
#define MAX31865_FAULT_STATUS_AUTO_CLEAR 1

struct max31865
{
	uint8_t config;
	uint16_t rtd;
	uint16_t fault_ht;
	uint16_t fault_lt;
	uint8_t cs_pin;
	uint8_t drdy_pin;
	uint8_t rtd_type;
	uint8_t initialized;
};
typedef struct max31865 max31865_t;

int max31865_init(max31865_t *m, const uint8_t cs_pin, const uint8_t drdy_pin,
		  const uint8_t rtd_type);
void max31865_cleanup(max31865_t *m);

uint8_t max31865_get_configuration(max31865_t *m);
uint8_t max31865_read_configuration(max31865_t *m);
int max31865_set_configuration(max31865_t *m, const int vbias,
			       const int conv_mode, const int one_shot,
			       const int rtd_type,
			       const int fault_detection_ctrl,
			       const int fault_status_clear,
			       const int noise_filter_frequency);

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
