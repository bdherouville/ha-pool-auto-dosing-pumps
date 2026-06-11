#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "pump_pwm.h"

/* Test-local mock for pump_pwm functions */

static struct {
	uint8_t duty;
	enum pump_dir dir;
	bool running;
} pump_state[PUMP_COUNT];

int pump_pwm_init(void)
{
	for (int i = 0; i < PUMP_COUNT; i++) {
		pump_state[i].duty = 0;
		pump_state[i].running = false;
	}
	return 0;
}

int pump_set(uint8_t idx, enum pump_dir dir, uint8_t duty)
{
	if (idx >= PUMP_COUNT) {
		return -EINVAL;
	}

	if (duty < 51) {
		/* Below threshold, stop the pump */
		pump_state[idx].duty = 0;
		pump_state[idx].running = false;
	} else if (duty <= 254) {
		pump_state[idx].duty = duty;
		pump_state[idx].dir = dir;
		pump_state[idx].running = true;
	} else {
		return -EINVAL;
	}

	return 0;
}

int pump_stop(uint8_t idx)
{
	if (idx >= PUMP_COUNT) {
		return -EINVAL;
	}

	pump_state[idx].duty = 0;
	pump_state[idx].running = false;

	return 0;
}

bool pump_is_running(uint8_t idx)
{
	if (idx >= PUMP_COUNT) {
		return false;
	}

	return pump_state[idx].running;
}
