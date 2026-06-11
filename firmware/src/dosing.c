#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "dosing.h"
#include "pump_pwm.h"

LOG_MODULE_REGISTER(dosing, LOG_LEVEL_INF);

#define PUMP_COUNT 4
#define MIN_DURATION_S 1
#define MAX_DURATION_S 3600
#define MIN_DUTY 51
#define MAX_DUTY 254

struct pump_state {
	struct k_timer timer;
	struct k_work work;
	uint16_t remaining_s;
	uint8_t duty;
	enum pump_dir dir;
	bool active;
};

static struct pump_state pump_states[PUMP_COUNT];
static struct k_mutex dosing_mutex;
static dosing_event_cb_t dosing_cb;

/* Work handler: runs in system workqueue context */
static void dosing_work_handler(struct k_work *work)
{
	struct pump_state *state = CONTAINER_OF(work, struct pump_state, work);
	uint8_t idx = state - pump_states;
	dosing_event_cb_t cb;

	k_mutex_lock(&dosing_mutex, K_FOREVER);

	if (!state->active) {
		k_mutex_unlock(&dosing_mutex);
		return;
	}

	/* Decrement remaining time */
	state->remaining_s--;

	/* Invoke callback outside the lock */
	cb = dosing_cb;
	k_mutex_unlock(&dosing_mutex);

	if (cb) {
		cb(idx, state->remaining_s, true);
	}

	/* Check if we've reached zero */
	k_mutex_lock(&dosing_mutex, K_FOREVER);
	if (state->remaining_s == 0) {
		pump_stop(idx);
		state->active = false;
		cb = dosing_cb;
		k_mutex_unlock(&dosing_mutex);

		if (cb) {
			cb(idx, 0, false);
		}
	} else {
		k_mutex_unlock(&dosing_mutex);
	}
}

/* Timer expiry handler: runs in ISR context, submits work to workqueue */
static void dosing_timer_expiry(struct k_timer *timer)
{
	struct pump_state *state = CONTAINER_OF(timer, struct pump_state, timer);
	k_work_submit(&state->work);
}

int dosing_init(dosing_event_cb_t cb)
{
	int ret;

	dosing_cb = cb;

	/* Initialize mutex */
	ret = k_mutex_init(&dosing_mutex);
	if (ret != 0) {
		return ret;
	}

	/* Initialize per-pump state */
	for (int i = 0; i < PUMP_COUNT; i++) {
		k_timer_init(&pump_states[i].timer, dosing_timer_expiry, NULL);
		k_work_init(&pump_states[i].work, dosing_work_handler);
		pump_states[i].remaining_s = 0;
		pump_states[i].active = false;
	}

	return 0;
}

int dosing_start(uint8_t idx, uint16_t duration_s, uint8_t duty, enum pump_dir dir)
{
	int ret;

	if (idx >= PUMP_COUNT) {
		return -EINVAL;
	}

	/* Clamp duration to [1, 3600] */
	if (duration_s < MIN_DURATION_S) {
		duration_s = MIN_DURATION_S;
	} else if (duration_s > MAX_DURATION_S) {
		duration_s = MAX_DURATION_S;
	}

	/* Clamp duty to [51, 254], or 0 to stop */
	if (duty > 0 && duty < MIN_DUTY) {
		duty = 0;  /* Stop if duty is below threshold */
	} else if (duty > MAX_DUTY) {
		duty = MAX_DUTY;
	}

	k_mutex_lock(&dosing_mutex, K_FOREVER);

	struct pump_state *state = &pump_states[idx];

	/* Call pump_set with the clamped values */
	ret = pump_set(idx, dir, duty);
	if (ret != 0) {
		k_mutex_unlock(&dosing_mutex);
		return ret;
	}

	/* Set state */
	state->remaining_s = duration_s;
	state->duty = duty;
	state->dir = dir;
	state->active = (duty > 0);

	if (state->active) {
		/* Start the timer with 1 second period */
		k_timer_start(&state->timer, K_SECONDS(1), K_SECONDS(1));
	} else {
		/* If duty was clamped to 0, stop any running timer */
		k_timer_stop(&state->timer);
	}

	k_mutex_unlock(&dosing_mutex);

	return 0;
}

int dosing_run(uint8_t idx, uint8_t duty, enum pump_dir dir)
{
	return dosing_start(idx, MAX_DURATION_S, duty, dir);
}

int dosing_abort(uint8_t idx)
{
	dosing_event_cb_t cb;

	if (idx >= PUMP_COUNT) {
		return -EINVAL;
	}

	k_mutex_lock(&dosing_mutex, K_FOREVER);

	struct pump_state *state = &pump_states[idx];
	bool was_active = state->active;

	/* Stop timer */
	k_timer_stop(&state->timer);

	/* Stop pump */
	pump_stop(idx);

	/* Clear state */
	state->active = false;
	state->remaining_s = 0;

	cb = dosing_cb;
	k_mutex_unlock(&dosing_mutex);

	/* Invoke callback only if it was active */
	if (was_active && cb) {
		cb(idx, 0, false);
	}

	return 0;
}

uint16_t dosing_remaining(uint8_t idx)
{
	uint16_t remaining;

	if (idx >= PUMP_COUNT) {
		return 0;
	}

	k_mutex_lock(&dosing_mutex, K_FOREVER);
	remaining = pump_states[idx].remaining_s;
	k_mutex_unlock(&dosing_mutex);

	return remaining;
}

bool dosing_active(uint8_t idx)
{
	bool active;

	if (idx >= PUMP_COUNT) {
		return false;
	}

	k_mutex_lock(&dosing_mutex, K_FOREVER);
	active = pump_states[idx].active;
	k_mutex_unlock(&dosing_mutex);

	return active;
}
