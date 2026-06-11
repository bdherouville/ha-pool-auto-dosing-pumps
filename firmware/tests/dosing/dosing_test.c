#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "dosing.h"
#include "pump_pwm.h"

/* Callback tracking */
static struct {
	uint8_t idx;
	uint16_t remaining;
	bool running;
	int count;
} last_callback;

static void test_callback(uint8_t idx, uint16_t remaining_s, bool running)
{
	last_callback.idx = idx;
	last_callback.remaining = remaining_s;
	last_callback.running = running;
	last_callback.count++;
}

ZTEST_SUITE(dosing, NULL, NULL, NULL, NULL, NULL);

/* Test: dosing_init */
ZTEST(dosing, test_dosing_init)
{
	int ret = dosing_init(test_callback);
	zassert_equal(ret, 0, "dosing_init should succeed");
}

/* Test: clamp duration_s: 0 -> 1s */
ZTEST(dosing, test_clamp_duration_min)
{
	dosing_init(test_callback);

	int ret = dosing_start(0, 0, 100, PUMP_DIR_FORWARD);
	zassert_equal(ret, 0, "dosing_start with 0s should be clamped and succeed");

	uint16_t remaining = dosing_remaining(0);
	zassert_equal(remaining, 1, "duration 0 should be clamped to 1s");

	dosing_abort(0);
}

/* Test: clamp duration_s: 5000 -> 3600s */
ZTEST(dosing, test_clamp_duration_max)
{
	dosing_init(test_callback);

	int ret = dosing_start(1, 5000, 100, PUMP_DIR_FORWARD);
	zassert_equal(ret, 0, "dosing_start with 5000s should be clamped and succeed");

	uint16_t remaining = dosing_remaining(1);
	zassert_equal(remaining, 3600, "duration 5000 should be clamped to 3600s");

	dosing_abort(1);
}

/* Test: clamp duty: 30 -> stop (duty < 51) */
ZTEST(dosing, test_clamp_duty_low)
{
	dosing_init(test_callback);

	int ret = dosing_start(2, 100, 30, PUMP_DIR_FORWARD);
	zassert_equal(ret, 0, "dosing_start with duty 30 should succeed");

	bool active = dosing_active(2);
	zassert_false(active, "pump should be stopped when duty < 51");

	dosing_abort(2);
}

/* Test: abort idempotency */
ZTEST(dosing, test_abort_idempotent)
{
	dosing_init(test_callback);

	/* Start a dose */
	dosing_start(0, 100, 100, PUMP_DIR_FORWARD);
	zassert_true(dosing_active(0), "pump should be active");

	/* Abort first time */
	last_callback.count = 0;
	int ret1 = dosing_abort(0);
	zassert_equal(ret1, 0, "first abort should succeed");
	zassert_equal(last_callback.count, 1, "callback should be invoked on first abort");

	/* Abort second time (idempotent) */
	last_callback.count = 0;
	int ret2 = dosing_abort(0);
	zassert_equal(ret2, 0, "second abort should succeed");
	zassert_equal(last_callback.count, 0, "callback should NOT be invoked on second abort (idempotent)");
}

/* Test: replace-while-active */
ZTEST(dosing, test_replace_while_active)
{
	dosing_init(test_callback);

	/* Start first dose */
	dosing_start(3, 100, 100, PUMP_DIR_FORWARD);
	zassert_true(dosing_active(3), "pump should be active");
	uint16_t remaining1 = dosing_remaining(3);

	/* Replace with a new dose */
	dosing_start(3, 50, 120, PUMP_DIR_REVERSE);
	zassert_true(dosing_active(3), "pump should still be active");
	uint16_t remaining2 = dosing_remaining(3);

	zassert_not_equal(remaining1, remaining2, "remaining time should be replaced");
	zassert_equal(remaining2, 50, "new dose should have 50s duration");

	dosing_abort(3);
}

/* Test: dosing_run (convenience function) */
ZTEST(dosing, test_dosing_run)
{
	dosing_init(test_callback);

	int ret = dosing_run(0, 100, PUMP_DIR_FORWARD);
	zassert_equal(ret, 0, "dosing_run should succeed");

	uint16_t remaining = dosing_remaining(0);
	zassert_equal(remaining, 3600, "dosing_run should set duration to 3600s");

	zassert_true(dosing_active(0), "pump should be active");

	dosing_abort(0);
}

/* Test: dosing_active after abort */
ZTEST(dosing, test_dosing_active_after_abort)
{
	dosing_init(test_callback);

	dosing_start(1, 100, 100, PUMP_DIR_FORWARD);
	zassert_true(dosing_active(1), "pump should be active");

	dosing_abort(1);
	zassert_false(dosing_active(1), "pump should be inactive after abort");
}

/* Test: invalid pump index */
ZTEST(dosing, test_invalid_pump_index)
{
	dosing_init(test_callback);

	int ret = dosing_start(PUMP_COUNT, 100, 100, PUMP_DIR_FORWARD);
	zassert_equal(ret, -EINVAL, "invalid index should return -EINVAL");

	uint16_t remaining = dosing_remaining(PUMP_COUNT);
	zassert_equal(remaining, 0, "remaining for invalid index should be 0");

	bool active = dosing_active(PUMP_COUNT);
	zassert_false(active, "active for invalid index should be false");
}
