#ifndef PUMP_PWM_H
#define PUMP_PWM_H

#include <stdint.h>
#include <stdbool.h>

#define PUMP_COUNT 4

enum pump_dir { PUMP_DIR_FORWARD = 0, PUMP_DIR_REVERSE = 1 };

/* Init all channels, force all pumps stopped. Call first in main(). */
int pump_pwm_init(void);

/* Run pump idx (0..3) at duty 0..254 in dir. duty < 20% of 254 (i.e. < 51) -> stop.
 * If direction differs from current running direction: stop, k_msleep(50), then start. */
int pump_set(uint8_t idx, enum pump_dir dir, uint8_t duty);

/* Both pins low. Idempotent. Safe from ISR? NO - thread context only. */
int pump_stop(uint8_t idx);

bool pump_is_running(uint8_t idx);

#endif /* PUMP_PWM_H */
