#ifndef ZB_PUMP_H
#define ZB_PUMP_H

#include <stdint.h>
#include <stdbool.h>

/* Pump custom cluster ID and manufacturer code */
#define PUMP_CLUSTER_ID			0xFC00
#define PUMP_CLUSTER_MANUF_CODE		0x1234

/* Pump custom cluster attribute IDs */
#define PUMP_ATTR_DIRECTION		0x0000
#define PUMP_ATTR_DOSE_DURATION_S	0x0001
#define PUMP_ATTR_DOSE_REMAINING_S	0x0002
#define PUMP_ATTR_FILL_TIME_1L_MIN_S	0x0003
#define PUMP_ATTR_FILL_TIME_1L_MAX_S	0x0004

/* Pump custom cluster command IDs */
#define PUMP_CMD_START_DOSE		0x00
#define PUMP_CMD_STOP_DOSE		0x01

/* For use in main.c dosing event marshalling */
#include "dosing.h"

/* Initialize pump Zigbee integration */
int zb_pump_init(void);

#endif /* ZB_PUMP_H */
