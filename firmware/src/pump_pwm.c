#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include "pump_pwm.h"

LOG_MODULE_REGISTER(pump_pwm, LOG_LEVEL_INF);

/* PWM spec for each channel */
static const struct pwm_dt_spec pump_specs[PUMP_COUNT][2] = {
	{
		PWM_DT_SPEC_GET(DT_ALIAS(pump1a)),
		PWM_DT_SPEC_GET(DT_ALIAS(pump1b)),
	},
	{
		PWM_DT_SPEC_GET(DT_ALIAS(pump2a)),
		PWM_DT_SPEC_GET(DT_ALIAS(pump2b)),
	},
	{
		PWM_DT_SPEC_GET(DT_ALIAS(pump3a)),
		PWM_DT_SPEC_GET(DT_ALIAS(pump3b)),
	},
	{
		PWM_DT_SPEC_GET(DT_ALIAS(pump4a)),
		PWM_DT_SPEC_GET(DT_ALIAS(pump4b)),
	},
};

/* State tracking per pump */
struct pump_state {
	bool running;
	enum pump_dir dir;
};

static struct pump_state pump_states[PUMP_COUNT] = {0};

int pump_pwm_init(void)
{
	int ret;

	/* Verify all 8 PWM channels are ready */
	for (int i = 0; i < PUMP_COUNT; i++) {
		if (!pwm_is_ready_dt(&pump_specs[i][0])) {
			LOG_ERR("Pump %d pin A not ready", i);
			return -ENODEV;
		}
		if (!pwm_is_ready_dt(&pump_specs[i][1])) {
			LOG_ERR("Pump %d pin B not ready", i);
			return -ENODEV;
		}
	}

	/* Set all channels to 0% duty (stopped) */
	for (int i = 0; i < PUMP_COUNT; i++) {
		ret = pwm_set_dt(&pump_specs[i][0], pump_specs[i][0].period, 0);
		if (ret != 0) {
			LOG_ERR("Failed to set pump %d pin A to 0: %d", i, ret);
			return ret;
		}

		ret = pwm_set_dt(&pump_specs[i][1], pump_specs[i][1].period, 0);
		if (ret != 0) {
			LOG_ERR("Failed to set pump %d pin B to 0: %d", i, ret);
			return ret;
		}

		pump_states[i].running = false;
		pump_states[i].dir = PUMP_DIR_FORWARD;
	}

	LOG_INF("Pump PWM initialized");
	return 0;
}

int pump_set(uint8_t idx, enum pump_dir dir, uint8_t duty)
{
	int ret;
	uint32_t pulse;

	if (idx >= PUMP_COUNT) {
		return -EINVAL;
	}

	/* Treat duty < 51 (20% of 254) as stop */
	if (duty < 51) {
		return pump_stop(idx);
	}

	/* Compute pulse width: duty / 254 of period
	 * pulse = (duty * period) / 254 */
	pulse = (uint32_t)duty * pump_specs[idx][0].period / 254;

	/* If currently running in opposite direction, stop, sleep, then apply */
	if (pump_states[idx].running && pump_states[idx].dir != dir) {
		LOG_INF("Pump %d direction change from %d to %d, stopping first", idx, pump_states[idx].dir, dir);
		pump_stop(idx);
		k_msleep(50);
	}

	if (dir == PUMP_DIR_FORWARD) {
		/* Forward: pin A at duty, pin B at 0 */
		ret = pwm_set_dt(&pump_specs[idx][0], pump_specs[idx][0].period, pulse);
		if (ret != 0) {
			LOG_ERR("Failed to set pump %d pin A: %d", idx, ret);
			return ret;
		}

		ret = pwm_set_dt(&pump_specs[idx][1], pump_specs[idx][1].period, 0);
		if (ret != 0) {
			LOG_ERR("Failed to set pump %d pin B to 0: %d", idx, ret);
			return ret;
		}
	} else {
		/* Reverse: pin A at 0, pin B at duty */
		ret = pwm_set_dt(&pump_specs[idx][0], pump_specs[idx][0].period, 0);
		if (ret != 0) {
			LOG_ERR("Failed to set pump %d pin A to 0: %d", idx, ret);
			return ret;
		}

		ret = pwm_set_dt(&pump_specs[idx][1], pump_specs[idx][1].period, pulse);
		if (ret != 0) {
			LOG_ERR("Failed to set pump %d pin B: %d", idx, ret);
			return ret;
		}
	}

	pump_states[idx].running = true;
	pump_states[idx].dir = dir;

	LOG_INF("Pump %d started: dir=%d, duty=%d", idx, dir, duty);

	return 0;
}

int pump_stop(uint8_t idx)
{
	int ret;

	if (idx >= PUMP_COUNT) {
		return -EINVAL;
	}

	/* Set both pins to 0% duty */
	ret = pwm_set_dt(&pump_specs[idx][0], pump_specs[idx][0].period, 0);
	if (ret != 0) {
		LOG_ERR("Failed to set pump %d pin A to 0: %d", idx, ret);
		return ret;
	}

	ret = pwm_set_dt(&pump_specs[idx][1], pump_specs[idx][1].period, 0);
	if (ret != 0) {
		LOG_ERR("Failed to set pump %d pin B to 0: %d", idx, ret);
		return ret;
	}

	if (pump_states[idx].running) {
		LOG_INF("Pump %d stopped", idx);
	}

	pump_states[idx].running = false;

	return 0;
}

bool pump_is_running(uint8_t idx)
{
	if (idx >= PUMP_COUNT) {
		return false;
	}

	return pump_states[idx].running;
}
