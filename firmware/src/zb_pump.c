#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <stdio.h>
#include <string.h>
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_app_utils.h>

#include "pump_pwm.h"
#include "dosing.h"
#include "zb_pump.h"

LOG_MODULE_REGISTER(zb_pump, LOG_LEVEL_INF);

/* HA profile ID and device ID as per spec */
#define PUMP_PROFILE_ID			ZB_AF_HA_PROFILE_ID
#define PUMP_DEVICE_ID			0x0101

/* Endpoints 1-4 map to pumps 0-3 */
#define PUMP_EP1			1
#define PUMP_EP2			2
#define PUMP_EP3			3
#define PUMP_EP4			4

/* Per-endpoint context storage for pump attributes */
typedef struct {
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_groups_attrs_t groups_attr;
	zb_zcl_on_off_attrs_t on_off_attr;
	zb_zcl_level_control_attrs_t level_control_attr;
	/* Extra Level Control attrs: Options (ExecuteIfOff) + StartUpCurrentLevel */
	uint8_t level_options;
	uint8_t level_start_up;
	/* Custom pump cluster attributes */
	uint8_t direction;
	uint16_t dose_duration_s;
	uint16_t dose_remaining_s;
	uint16_t fill_time_1l_min_s;
	uint16_t fill_time_1l_max_s;
} pump_ep_ctx_t;

/* Device context for all 4 endpoints */
typedef struct {
	pump_ep_ctx_t ep1;
	pump_ep_ctx_t ep2;
	pump_ep_ctx_t ep3;
	pump_ep_ctx_t ep4;
} pump_device_ctx_t;

static pump_device_ctx_t dev_ctx;

/* Helper to get endpoint context by endpoint number */
static pump_ep_ctx_t *get_ep_ctx(uint8_t ep)
{
	switch (ep) {
	case PUMP_EP1:
		return &dev_ctx.ep1;
	case PUMP_EP2:
		return &dev_ctx.ep2;
	case PUMP_EP3:
		return &dev_ctx.ep3;
	case PUMP_EP4:
		return &dev_ctx.ep4;
	default:
		return NULL;
	}
}

/* Helper to map endpoint to pump index */
static uint8_t ep_to_pump_idx(uint8_t ep)
{
	return ep - 1;
}

/* Custom cluster init stubs - required by ZB_ZCL_CLUSTER_DESC macro */
static void pump_custom_cluster_init_server(void) { }
static void pump_custom_cluster_init_client(void) { }

#define PUMP_CLUSTER_ID_SERVER_ROLE_INIT pump_custom_cluster_init_server
#define PUMP_CLUSTER_ID_CLIENT_ROLE_INIT pump_custom_cluster_init_client

/* ============================================================================
 * Custom cluster attribute list definition - per-endpoint
 * ============================================================================
 */

/* Cluster revision for custom cluster */
static zb_uint16_t pump_custom_cluster_revision = 0x0001u;

/* Helper macro to declare per-endpoint custom cluster attribute lists */
#define DECLARE_PUMP_EP_CUSTOM_ATTRS(n) \
	static zb_zcl_attr_t pump_custom_attr_list_##n[] = { \
		{ \
			PUMP_ATTR_DIRECTION, \
			ZB_ZCL_ATTR_TYPE_8BIT_ENUM, \
			ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_MANUF_SPEC, \
			PUMP_CLUSTER_MANUF_CODE, \
			&dev_ctx.ep##n.direction \
		}, \
		{ \
			PUMP_ATTR_DOSE_DURATION_S, \
			ZB_ZCL_ATTR_TYPE_U16, \
			ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_MANUF_SPEC, \
			PUMP_CLUSTER_MANUF_CODE, \
			&dev_ctx.ep##n.dose_duration_s \
		}, \
		{ \
			PUMP_ATTR_DOSE_REMAINING_S, \
			ZB_ZCL_ATTR_TYPE_U16, \
			ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING | ZB_ZCL_ATTR_MANUF_SPEC, \
			PUMP_CLUSTER_MANUF_CODE, \
			&dev_ctx.ep##n.dose_remaining_s \
		}, \
		{ \
			PUMP_ATTR_FILL_TIME_1L_MIN_S, \
			ZB_ZCL_ATTR_TYPE_U16, \
			ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_ACCESS_REPORTING | ZB_ZCL_ATTR_MANUF_SPEC, \
			PUMP_CLUSTER_MANUF_CODE, \
			&dev_ctx.ep##n.fill_time_1l_min_s \
		}, \
		{ \
			PUMP_ATTR_FILL_TIME_1L_MAX_S, \
			ZB_ZCL_ATTR_TYPE_U16, \
			ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_ACCESS_REPORTING | ZB_ZCL_ATTR_MANUF_SPEC, \
			PUMP_CLUSTER_MANUF_CODE, \
			&dev_ctx.ep##n.fill_time_1l_max_s \
		}, \
		{ \
			ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, \
			ZB_ZCL_ATTR_TYPE_U16, \
			ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
			ZB_ZCL_MANUF_CODE_INVALID, \
			&pump_custom_cluster_revision \
		} \
	}

/* Declare custom cluster attribute lists for each endpoint */
DECLARE_PUMP_EP_CUSTOM_ATTRS(1);
DECLARE_PUMP_EP_CUSTOM_ATTRS(2);
DECLARE_PUMP_EP_CUSTOM_ATTRS(3);
DECLARE_PUMP_EP_CUSTOM_ATTRS(4);

/* ============================================================================
 * Standard cluster attribute lists - using ZBOSS macros (per-endpoint)
 * ============================================================================
 */

/* Helper macro to declare per-endpoint standard cluster attribute lists */
#define DECLARE_PUMP_EP_ATTRS(n) \
	ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(pump_basic_list_##n, \
		&dev_ctx.ep##n.basic_attr.zcl_version, \
		&dev_ctx.ep##n.basic_attr.app_version, \
		&dev_ctx.ep##n.basic_attr.stack_version, \
		&dev_ctx.ep##n.basic_attr.hw_version, \
		dev_ctx.ep##n.basic_attr.mf_name, \
		dev_ctx.ep##n.basic_attr.model_id, \
		dev_ctx.ep##n.basic_attr.date_code, \
		&dev_ctx.ep##n.basic_attr.power_source, \
		dev_ctx.ep##n.basic_attr.location_id, \
		&dev_ctx.ep##n.basic_attr.ph_env, \
		dev_ctx.ep##n.basic_attr.sw_ver); \
	ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(pump_identify_list_##n, \
		&dev_ctx.ep##n.identify_attr.identify_time); \
	ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(pump_groups_list_##n, \
		&dev_ctx.ep##n.groups_attr.name_support); \
	ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST(pump_on_off_list_##n, \
		&dev_ctx.ep##n.on_off_attr.on_off); \
	ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST_EXT(pump_level_control_list_##n, \
		&dev_ctx.ep##n.level_control_attr.current_level, \
		&dev_ctx.ep##n.level_control_attr.remaining_time, \
		&dev_ctx.ep##n.level_start_up, \
		&dev_ctx.ep##n.level_options)

/* Declare attribute lists for each endpoint */
DECLARE_PUMP_EP_ATTRS(1);
DECLARE_PUMP_EP_ATTRS(2);
DECLARE_PUMP_EP_ATTRS(3);
DECLARE_PUMP_EP_ATTRS(4);

/* ============================================================================
 * Cluster lists for each endpoint
 * ============================================================================
 */

/* EP1 cluster list (includes Basic cluster) */
zb_zcl_cluster_desc_t pump_ep1_clusters[] = {
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_BASIC,
		ZB_ZCL_ARRAY_SIZE(pump_basic_list_1, zb_zcl_attr_t),
		pump_basic_list_1,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_ARRAY_SIZE(pump_identify_list_1, zb_zcl_attr_t),
		pump_identify_list_1,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_ARRAY_SIZE(pump_on_off_list_1, zb_zcl_attr_t),
		pump_on_off_list_1,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		ZB_ZCL_ARRAY_SIZE(pump_level_control_list_1, zb_zcl_attr_t),
		pump_level_control_list_1,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		PUMP_CLUSTER_ID,
		ZB_ZCL_ARRAY_SIZE(pump_custom_attr_list_1, zb_zcl_attr_t),
		pump_custom_attr_list_1,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		PUMP_CLUSTER_MANUF_CODE)
};

/* EP2 cluster list */
zb_zcl_cluster_desc_t pump_ep2_clusters[] = {
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_ARRAY_SIZE(pump_identify_list_2, zb_zcl_attr_t),
		pump_identify_list_2,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_ARRAY_SIZE(pump_on_off_list_2, zb_zcl_attr_t),
		pump_on_off_list_2,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		ZB_ZCL_ARRAY_SIZE(pump_level_control_list_2, zb_zcl_attr_t),
		pump_level_control_list_2,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		PUMP_CLUSTER_ID,
		ZB_ZCL_ARRAY_SIZE(pump_custom_attr_list_2, zb_zcl_attr_t),
		pump_custom_attr_list_2,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		PUMP_CLUSTER_MANUF_CODE)
};

/* EP3 cluster list */
zb_zcl_cluster_desc_t pump_ep3_clusters[] = {
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_ARRAY_SIZE(pump_identify_list_3, zb_zcl_attr_t),
		pump_identify_list_3,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_ARRAY_SIZE(pump_on_off_list_3, zb_zcl_attr_t),
		pump_on_off_list_3,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		ZB_ZCL_ARRAY_SIZE(pump_level_control_list_3, zb_zcl_attr_t),
		pump_level_control_list_3,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		PUMP_CLUSTER_ID,
		ZB_ZCL_ARRAY_SIZE(pump_custom_attr_list_3, zb_zcl_attr_t),
		pump_custom_attr_list_3,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		PUMP_CLUSTER_MANUF_CODE)
};

/* EP4 cluster list */
zb_zcl_cluster_desc_t pump_ep4_clusters[] = {
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_ARRAY_SIZE(pump_identify_list_4, zb_zcl_attr_t),
		pump_identify_list_4,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_ARRAY_SIZE(pump_on_off_list_4, zb_zcl_attr_t),
		pump_on_off_list_4,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		ZB_ZCL_ARRAY_SIZE(pump_level_control_list_4, zb_zcl_attr_t),
		pump_level_control_list_4,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(
		PUMP_CLUSTER_ID,
		ZB_ZCL_ARRAY_SIZE(pump_custom_attr_list_4, zb_zcl_attr_t),
		pump_custom_attr_list_4,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		PUMP_CLUSTER_MANUF_CODE)
};

/* ============================================================================
 * Simple descriptors for each endpoint
 * ============================================================================
 */

/* Declare simple descriptor types (guard against redefinition) */
ZB_DECLARE_SIMPLE_DESC(5, 0);
ZB_DECLARE_SIMPLE_DESC(4, 0);

/* EP1 simple descriptor: 5 input clusters (Basic, Identify, OnOff, Level, Custom), 0 output */
ZB_AF_SIMPLE_DESC_TYPE(5, 0) pump_ep1_simple_desc = {
	PUMP_EP1,                       /* endpoint */
	PUMP_PROFILE_ID,                /* profile (0x0104 = HA) */
	PUMP_DEVICE_ID,                 /* device_id */
	0,                              /* device_version */
	0,                              /* reserved */
	5,                              /* in_cluster_count */
	0,                              /* out_cluster_count */
	{
		ZB_ZCL_CLUSTER_ID_BASIC,
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		PUMP_CLUSTER_ID
	}
};

/* EP2 simple descriptor: 4 input clusters (Identify, OnOff, Level, Custom), 0 output */
ZB_AF_SIMPLE_DESC_TYPE(4, 0) pump_ep2_simple_desc = {
	PUMP_EP2,                       /* endpoint */
	PUMP_PROFILE_ID,                /* profile */
	PUMP_DEVICE_ID,                 /* device_id */
	0,                              /* device_version */
	0,                              /* reserved */
	4,                              /* in_cluster_count */
	0,                              /* out_cluster_count */
	{
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		PUMP_CLUSTER_ID
	}
};

/* EP3 simple descriptor: 4 input clusters (Identify, OnOff, Level, Custom), 0 output */
ZB_AF_SIMPLE_DESC_TYPE(4, 0) pump_ep3_simple_desc = {
	PUMP_EP3,                       /* endpoint */
	PUMP_PROFILE_ID,                /* profile */
	PUMP_DEVICE_ID,                 /* device_id */
	0,                              /* device_version */
	0,                              /* reserved */
	4,                              /* in_cluster_count */
	0,                              /* out_cluster_count */
	{
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		PUMP_CLUSTER_ID
	}
};

/* EP4 simple descriptor: 4 input clusters (Identify, OnOff, Level, Custom), 0 output */
ZB_AF_SIMPLE_DESC_TYPE(4, 0) pump_ep4_simple_desc = {
	PUMP_EP4,                       /* endpoint */
	PUMP_PROFILE_ID,                /* profile */
	PUMP_DEVICE_ID,                 /* device_id */
	0,                              /* device_version */
	0,                              /* reserved */
	4,                              /* in_cluster_count */
	0,                              /* out_cluster_count */
	{
		ZB_ZCL_CLUSTER_ID_IDENTIFY,
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
		PUMP_CLUSTER_ID
	}
};

/* ============================================================================
 * Endpoint declarations
 * ============================================================================
 */

/* Per-endpoint contexts required by ZBOSS:
 * - reporting slots: OnOff, CurrentLevel + 3 reportable custom attrs
 * - CVC slot: Level Control Move-to-Level transition engine (1 per EP) */
#define PUMP_EP_REPORT_ATTR_COUNT 6

ZBOSS_DEVICE_DECLARE_REPORTING_CTX(pump_ep1_reporting_info, PUMP_EP_REPORT_ATTR_COUNT);
ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(pump_ep1_cvc_alarm_info, 1);
ZB_AF_DECLARE_ENDPOINT_DESC(pump_ep1, PUMP_EP1, PUMP_PROFILE_ID, 0, NULL,
	ZB_ZCL_ARRAY_SIZE(pump_ep1_clusters, zb_zcl_cluster_desc_t),
	pump_ep1_clusters,
	(zb_af_simple_desc_1_1_t *)&pump_ep1_simple_desc,
	PUMP_EP_REPORT_ATTR_COUNT, pump_ep1_reporting_info,
	1, pump_ep1_cvc_alarm_info);

ZBOSS_DEVICE_DECLARE_REPORTING_CTX(pump_ep2_reporting_info, PUMP_EP_REPORT_ATTR_COUNT);
ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(pump_ep2_cvc_alarm_info, 1);
ZB_AF_DECLARE_ENDPOINT_DESC(pump_ep2, PUMP_EP2, PUMP_PROFILE_ID, 0, NULL,
	ZB_ZCL_ARRAY_SIZE(pump_ep2_clusters, zb_zcl_cluster_desc_t),
	pump_ep2_clusters,
	(zb_af_simple_desc_1_1_t *)&pump_ep2_simple_desc,
	PUMP_EP_REPORT_ATTR_COUNT, pump_ep2_reporting_info,
	1, pump_ep2_cvc_alarm_info);

ZBOSS_DEVICE_DECLARE_REPORTING_CTX(pump_ep3_reporting_info, PUMP_EP_REPORT_ATTR_COUNT);
ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(pump_ep3_cvc_alarm_info, 1);
ZB_AF_DECLARE_ENDPOINT_DESC(pump_ep3, PUMP_EP3, PUMP_PROFILE_ID, 0, NULL,
	ZB_ZCL_ARRAY_SIZE(pump_ep3_clusters, zb_zcl_cluster_desc_t),
	pump_ep3_clusters,
	(zb_af_simple_desc_1_1_t *)&pump_ep3_simple_desc,
	PUMP_EP_REPORT_ATTR_COUNT, pump_ep3_reporting_info,
	1, pump_ep3_cvc_alarm_info);

ZBOSS_DEVICE_DECLARE_REPORTING_CTX(pump_ep4_reporting_info, PUMP_EP_REPORT_ATTR_COUNT);
ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(pump_ep4_cvc_alarm_info, 1);
ZB_AF_DECLARE_ENDPOINT_DESC(pump_ep4, PUMP_EP4, PUMP_PROFILE_ID, 0, NULL,
	ZB_ZCL_ARRAY_SIZE(pump_ep4_clusters, zb_zcl_cluster_desc_t),
	pump_ep4_clusters,
	(zb_af_simple_desc_1_1_t *)&pump_ep4_simple_desc,
	PUMP_EP_REPORT_ATTR_COUNT, pump_ep4_reporting_info,
	1, pump_ep4_cvc_alarm_info);

/* Device context with 4 endpoints */
ZBOSS_DECLARE_DEVICE_CTX_4_EP(pump_device_ctx, pump_ep1, pump_ep2, pump_ep3, pump_ep4);

/* ============================================================================
 * Endpoint handler for custom cluster commands
 * ============================================================================
 */

static zb_uint8_t pump_ep_handler(zb_uint8_t param)
{
	zb_zcl_parsed_hdr_t *hdr = (zb_zcl_parsed_hdr_t *)ZB_BUF_GET_PARAM(param, zb_zcl_parsed_hdr_t);
	zb_uint8_t *payload = zb_buf_begin(param);
	zb_uint16_t payload_len = zb_buf_len(param);
	uint8_t endpoint = hdr->addr_data.common_data.dst_endpoint;
	uint8_t pump_idx = ep_to_pump_idx(endpoint);
	pump_ep_ctx_t *ep_ctx = get_ep_ctx(endpoint);

	/* Only handle our custom cluster */
	if (hdr->cluster_id != PUMP_CLUSTER_ID) {
		return ZB_FALSE;
	}

	/* Verify manufacturer code */
	if (!hdr->is_manuf_specific || hdr->manuf_specific != PUMP_CLUSTER_MANUF_CODE) {
		return ZB_FALSE;
	}

	LOG_INF("Custom cluster command: EP%d cmd_id=%d payload_len=%d",
		endpoint, hdr->cmd_id, payload_len);

	switch (hdr->cmd_id) {
	case PUMP_CMD_START_DOSE: {
		/* start_dose: uint16 duration_s, uint8 level, uint8 direction */
		if (payload_len < 4) {
			LOG_WRN("Invalid start_dose payload length: %d", payload_len);
			return ZB_FALSE;
		}

		uint16_t duration_s = payload[0] | (payload[1] << 8);
		uint8_t level = payload[2];
		uint8_t direction = payload[3];

		/* Clamp duration to 1-3600 */
		if (duration_s < 1) {
			duration_s = 1;
		} else if (duration_s > 3600) {
			duration_s = 3600;
		}

		LOG_INF("start_dose: pump %d, duration %d s, level %d, dir %d",
			pump_idx, duration_s, level, direction);

		/* Update attributes */
		if (ep_ctx) {
			ep_ctx->direction = direction;
			ep_ctx->dose_duration_s = duration_s;
		}

		/* Start dose */
		dosing_start(pump_idx, duration_s, level, direction);

		zb_buf_free(param);
		return ZB_TRUE;
	}

	case PUMP_CMD_STOP_DOSE:
		LOG_INF("stop_dose: pump %d", pump_idx);
		dosing_abort(pump_idx);
		zb_buf_free(param);
		return ZB_TRUE;

	default:
		LOG_WRN("Unknown custom cluster command: %d", hdr->cmd_id);
		return ZB_FALSE;
	}
}

/* ============================================================================
 * Settings subsystem handler for persistence
 * ============================================================================
 */

static int pump_settings_handler(const char *key, size_t len,
				  settings_read_cb read_cb, void *cb_arg)
{
	/* Parse key format: pump/cal/<ep>/min or pump/cal/<ep>/max */
	if (strncmp(key, "cal/", 4) != 0) {
		return -ENOENT;
	}

	const char *ep_str = key + 4;
	const char *slash = strchr(ep_str, '/');
	if (!slash) {
		return -ENOENT;
	}

	char ep_num = ep_str[0];
	uint8_t ep = (ep_num >= '1' && ep_num <= '4') ? (ep_num - '0') : 0;
	if (ep < 1 || ep > 4) {
		return -ENOENT;
	}

	const char *attr_type = slash + 1;
	pump_ep_ctx_t *ep_ctx = get_ep_ctx(ep);
	if (!ep_ctx) {
		return -ENOENT;
	}

	uint16_t value = 0;
	ssize_t bytes_read = read_cb(cb_arg, &value, sizeof(value));
	if (bytes_read < 0) {
		return bytes_read;
	}

	if (strcmp(attr_type, "min") == 0) {
		ep_ctx->fill_time_1l_min_s = value;
		LOG_INF("Loaded calibration min for EP%d: %d s", ep, value);
	} else if (strcmp(attr_type, "max") == 0) {
		ep_ctx->fill_time_1l_max_s = value;
		LOG_INF("Loaded calibration max for EP%d: %d s", ep, value);
	}

	return 0;
}

static struct settings_handler pump_settings = {
	.name = "pump",
	.h_get = NULL,
	.h_set = pump_settings_handler,
	.h_commit = NULL,
	.h_export = NULL,
};

/* ============================================================================
 * ZCL device callback for handling standard cluster commands
 * ============================================================================
 */

/* Apply a speed (level) change. Level 0 pauses the pump but keeps the
 * remaining run time; raising the level again resumes it (OnOff still on). */
static void pump_apply_level(uint8_t ep, uint8_t new_level)
{
	pump_ep_ctx_t *ep_ctx = get_ep_ctx(ep);
	uint8_t pump_idx = ep_to_pump_idx(ep);

	if (!ep_ctx) {
		return;
	}

	ep_ctx->level_control_attr.current_level = new_level;

	if (dosing_active(pump_idx)) {
		/* Running: re-apply, keeping the remaining time (0 pauses) */
		dosing_start(pump_idx, dosing_remaining(pump_idx),
			     new_level, ep_ctx->direction);
	} else if (new_level > 0 && ep_ctx->on_off_attr.on_off) {
		/* Paused at 0%: resume with the paused countdown, if any */
		uint16_t rem = dosing_remaining(pump_idx);

		dosing_start(pump_idx, rem ? rem : 3600,
			     new_level, ep_ctx->direction);
	}
}

static void zcl_device_cb(zb_bufid_t bufid)
{
	zb_zcl_device_callback_param_t *device_cb_param =
		ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
	zb_uint8_t ep = device_cb_param->endpoint;
	zb_uint16_t cluster_id = device_cb_param->cb_param.set_attr_value_param.cluster_id;
	zb_uint16_t attr_id = device_cb_param->cb_param.set_attr_value_param.attr_id;
	uint8_t pump_idx = ep_to_pump_idx(ep);

	device_cb_param->status = RET_OK;

	LOG_DBG("zcl_device_cb: id=%d ep=%d cluster=0x%04x attr=0x%04x",
		device_cb_param->device_cb_id, ep, cluster_id, attr_id);

	switch (device_cb_param->device_cb_id) {
	case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
		/* Handle On/Off cluster */
		if (cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF) {
			uint8_t value = device_cb_param->cb_param.set_attr_value_param.values.data8;
			LOG_INF("EP%d OnOff set to %d", ep, value);

			if (attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
				if (value) {
					/* On: start dose at current level and direction */
					pump_ep_ctx_t *ep_ctx = get_ep_ctx(ep);
					if (ep_ctx) {
						dosing_run(pump_idx, ep_ctx->level_control_attr.current_level, ep_ctx->direction);
					}
				} else {
					/* Off: abort dose */
					dosing_abort(pump_idx);
				}
			}
		}
		/* Handle Level Control cluster */
		else if (cluster_id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
			uint8_t new_level = device_cb_param->cb_param.set_attr_value_param.values.data8;
			LOG_INF("EP%d Level set to %d", ep, new_level);

			if (attr_id == ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
				pump_apply_level(ep, new_level);
			}
		}
		/* Handle custom pump cluster attributes */
		else if (cluster_id == PUMP_CLUSTER_ID) {
			if (attr_id == PUMP_ATTR_DIRECTION) {
				uint8_t value = device_cb_param->cb_param.set_attr_value_param.values.data8;
				pump_ep_ctx_t *ep_ctx = get_ep_ctx(ep);

				LOG_INF("EP%d direction set to %d", ep, value);
				if (ep_ctx) {
					ep_ctx->direction = value;
					/* Apply immediately if running (safe reverse in
					 * pump_pwm), keeping the remaining dose time */
					if (dosing_active(pump_idx)) {
						dosing_start(pump_idx, dosing_remaining(pump_idx),
							     ep_ctx->level_control_attr.current_level,
							     value);
					}
				}
			} else if (attr_id == PUMP_ATTR_FILL_TIME_1L_MIN_S ||
			    attr_id == PUMP_ATTR_FILL_TIME_1L_MAX_S) {
				uint16_t value = device_cb_param->cb_param.set_attr_value_param.values.data16;
				const char *attr_name = (attr_id == PUMP_ATTR_FILL_TIME_1L_MIN_S) ? "min" : "max";
				LOG_INF("EP%d calibration %s set to %d s", ep, attr_name, value);

				/* Persist to settings: pump/cal/<ep>/min or pump/cal/<ep>/max */
				char key[32];
				snprintf(key, sizeof(key), "pump/cal/%d/%s", ep, attr_name);
				int ret = settings_save_one(key, &value, sizeof(value));
				if (ret != 0) {
					LOG_WRN("Failed to persist calibration: %d", ret);
				}
			}
		}
		break;

	case ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID: {
		/* Move-to-Level commands arrive here, not via SET_ATTR_VALUE */
		uint8_t new_level = device_cb_param->cb_param.level_control_set_value_param.new_value;

		LOG_INF("EP%d Level set to %d (level-control cmd)", ep, new_level);
		pump_apply_level(ep, new_level);
		break;
	}

	default:
		device_cb_param->status = RET_NOT_IMPLEMENTED;
		break;
	}
}

/* ============================================================================
 * Initialization
 * ============================================================================
 */

int zb_pump_init(void)
{
	LOG_INF("Initializing pump Zigbee layer");

	/* Initialize attribute values */
	/* Basic cluster - EP1 only */
	dev_ctx.ep1.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.ep1.basic_attr.app_version = 1;
	dev_ctx.ep1.basic_attr.stack_version = 1;
	dev_ctx.ep1.basic_attr.hw_version = 1;
	dev_ctx.ep1.basic_attr.power_source = 0x01; /* Mains power */
	dev_ctx.ep1.basic_attr.ph_env = 0;
	strcpy((char *)dev_ctx.ep1.basic_attr.mf_name, "\x03""DIY");
	strcpy((char *)dev_ctx.ep1.basic_attr.model_id, "\x0D""PISCINE-PUMP4");

	/* Identify cluster - all endpoints */
	dev_ctx.ep1.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
	dev_ctx.ep1.groups_attr.name_support = 0;
	dev_ctx.ep1.on_off_attr.on_off = ZB_FALSE;
	/* Default to full speed so bare On works before any level is set */
	dev_ctx.ep1.level_control_attr.current_level = 254;
	dev_ctx.ep1.level_control_attr.remaining_time = 0;
	/* ExecuteIfOff: accept speed changes while the pump is stopped
	 * (ZCL8 3.10.2.2.8.1 otherwise drops MoveToLevel when OnOff=false) */
	dev_ctx.ep1.level_options = 1U << ZB_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF;
	dev_ctx.ep1.level_start_up = 254;
	dev_ctx.ep1.direction = PUMP_DIR_FORWARD;
	dev_ctx.ep1.dose_duration_s = 0;
	dev_ctx.ep1.dose_remaining_s = 0;
	dev_ctx.ep1.fill_time_1l_min_s = 0;
	dev_ctx.ep1.fill_time_1l_max_s = 0;

	/* Copy EP1 context to EP2-4 */
	dev_ctx.ep2 = dev_ctx.ep1;
	dev_ctx.ep3 = dev_ctx.ep1;
	dev_ctx.ep4 = dev_ctx.ep1;

	/* Register settings handler for persistence */
	settings_register(&pump_settings);

	/* Load persisted calibration values from settings */
	settings_load_subtree("pump");

	/* Register device context */
	ZB_AF_REGISTER_DEVICE_CTX(&pump_device_ctx);

	/* Register ZCL device callback for standard cluster commands */
	ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);

	/* Register endpoint handlers for custom cluster command processing */
	ZB_AF_SET_ENDPOINT_HANDLER(PUMP_EP1, pump_ep_handler);
	ZB_AF_SET_ENDPOINT_HANDLER(PUMP_EP2, pump_ep_handler);
	ZB_AF_SET_ENDPOINT_HANDLER(PUMP_EP3, pump_ep_handler);
	ZB_AF_SET_ENDPOINT_HANDLER(PUMP_EP4, pump_ep_handler);

	return 0;
}
