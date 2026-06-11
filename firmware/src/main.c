#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zigbee/zigbee_app_utils.h>

#include "pump_pwm.h"
#include "dosing.h"
#include "zb_pump.h"

LOG_MODULE_REGISTER(piscine, LOG_LEVEL_INF);

/* Dosing event info for marshalling to Zigbee thread */
typedef struct {
	uint8_t idx;           /* Pump index 0-3 */
	uint16_t remaining_s;  /* Remaining dose time */
	uint8_t running;       /* 1 if running, 0 if stopped */
} dosing_event_t;

/* Storage for pending dosing event (only one at a time per pump) */
static dosing_event_t pending_events[4];

/* Callback function to be executed in Zigbee thread context */
static void dosing_event_handler(zb_bufid_t bufid)
{
	(void)bufid; /* Unused */

	/* Process pending events for all pumps */
	for (uint8_t idx = 0; idx < 4; idx++) {
		dosing_event_t *event = &pending_events[idx];
		uint8_t ep = idx + 1; /* Convert pump index to endpoint */

		LOG_INF("Dosing event in Zigbee context: pump %d, remaining %d s, running %d",
			idx, event->remaining_s, event->running);

		/* Update OnOff attribute (0x0006 cluster) */
		ZB_ZCL_SET_ATTRIBUTE(
			ep,
			ZB_ZCL_CLUSTER_ID_ON_OFF,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
			(zb_uint8_t *)&event->running,
			ZB_TRUE);

		/* Update dose_remaining_s attribute (0xFC00 custom cluster, attr 0x0002) */
		ZB_ZCL_SET_ATTRIBUTE(
			ep,
			PUMP_CLUSTER_ID,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			PUMP_ATTR_DOSE_REMAINING_S,
			(zb_uint8_t *)&event->remaining_s,
			ZB_TRUE);
	}
}

/* Dosing event callback - bridges dosing thread to Zigbee context */
static void dosing_callback(uint8_t idx, uint16_t remaining_s, bool running)
{
	LOG_DBG("Dosing callback: pump %d, remaining %d s, running %d", idx, remaining_s, running);

	/* Store event data */
	if (idx < 4) {
		pending_events[idx].idx = idx;
		pending_events[idx].remaining_s = remaining_s;
		pending_events[idx].running = (uint8_t)running;

		/* Schedule callback on Zigbee thread */
		ZB_SCHEDULE_APP_CALLBACK(dosing_event_handler, 0);
	}
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	/* Handle Zigbee signals using NCS default handler */
	zb_ret_t zb_err_code = zigbee_default_signal_handler(bufid);

	if (zb_err_code != RET_OK) {
		LOG_WRN("Zigbee signal handler error: %d", zb_err_code);
	}

	if (bufid) {
		zb_buf_free(bufid);
	}
}

int main(void)
{
	int err;

	LOG_INF("piscine pump4 boot");

	/* USB-powered (VDDH) boards: factory/erased UICR REGOUT0 leaves the
	 * GPIO rail at 1.8 V, too low for L298N logic-high. Program 3.3 V
	 * once and reset (same approach as Nordic's nrf52840dongle board). */
	if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) !=
	    (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos)) {
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
		while (!NRF_NVMC->READY) {
		}
		NRF_UICR->REGOUT0 =
			(NRF_UICR->REGOUT0 & ~UICR_REGOUT0_VOUT_Msk) |
			(UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);
		while (!NRF_NVMC->READY) {
		}
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
		while (!NRF_NVMC->READY) {
		}
		LOG_WRN("UICR REGOUT0 set to 3.3 V, resetting");
		NVIC_SystemReset();
	}

	/* SuperMini nRF52840: P0.13 gates the external 3V3/VCC rail
	 * (low = off). Latch it high so the VCC pin is powered. */
	err = gpio_pin_configure(DEVICE_DT_GET(DT_NODELABEL(gpio0)), 13,
				 GPIO_OUTPUT_HIGH);
	if (err) {
		LOG_WRN("ext-VCC enable (P0.13) failed: %d", err);
	}

	/* Initialize pump PWM driver */
	err = pump_pwm_init();
	if (err) {
		LOG_ERR("pump_pwm_init failed: %d", err);
		return err;
	}

	/* Initialize dosing engine with callback */
	err = dosing_init(dosing_callback);
	if (err) {
		LOG_ERR("dosing_init failed: %d", err);
		return err;
	}

	/* Initialize Zigbee pump layer (registers device context and clusters) */
	err = zb_pump_init();
	if (err) {
		LOG_ERR("zb_pump_init failed: %d", err);
		return err;
	}

	/* Start Zigbee thread and enable the stack */
	zigbee_enable();

	LOG_INF("piscine pump4 initialized");

	/* Main loop */
	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
