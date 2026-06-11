#ifndef DOSING_H
#define DOSING_H

#include <stdint.h>
#include <stdbool.h>
#include "pump_pwm.h"

typedef void (*dosing_event_cb_t)(uint8_t idx, uint16_t remaining_s, bool running);

int dosing_init(dosing_event_cb_t cb);
int dosing_start(uint8_t idx, uint16_t duration_s, uint8_t duty, enum pump_dir dir);
int dosing_run(uint8_t idx, uint8_t duty, enum pump_dir dir);   /* = dosing_start(idx, 3600, ...) */
int dosing_abort(uint8_t idx);
uint16_t dosing_remaining(uint8_t idx);
bool dosing_active(uint8_t idx);

#endif /* DOSING_H */
