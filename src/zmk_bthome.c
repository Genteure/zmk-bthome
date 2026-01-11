/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/random/random.h>

#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#include <zmk_bthome/zmk_bthome.h>
#include <dt-bindings/zmk_bthome/button.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct zmk_bthome_obj8
{
    const uint8_t obj_id;
    uint8_t data;
} __packed;

struct zmk_bthome_obj16
{
    const uint8_t obj_id;
    uint16_t data;
} __packed;

// Number of buttons placeholder, to be replaced with actual number from devicetree
#define BTHOME_BUTTON_NUM DT_NUM_INST_STATUS_OKAY(zmk_behavior_bthome_button)

// uuid(2) + device_info(1)
#define PAYLOAD_CONTENT_OFFSET 3
#define PAYLOAD_CONTENT_SIZE (sizeof(struct zmk_bthome_payload) - PAYLOAD_CONTENT_OFFSET)

struct zmk_bthome_payload
{
    const uint8_t uuid[2];
    const uint8_t device_info;
    struct zmk_bthome_obj8 packet_id;
    struct zmk_bthome_obj8 battery_level;
    struct zmk_bthome_obj16 battery_voltage;
    struct zmk_bthome_obj8 buttons[BTHOME_BUTTON_NUM];
} __packed;

static union
{
    struct zmk_bthome_payload data;
    uint8_t bytes[sizeof(struct zmk_bthome_payload)];
} bthome_payload = {
    .data = {
        .uuid = {BT_UUID_16_ENCODE(ZMK_BTHOME_SERVICE_UUID)},
        .device_info = ZMK_BTHOME_DEVICE_INFO,
        .packet_id = {.obj_id = ZMK_BTHOME_OBJECT_ID_PACKET_ID, .data = 0},
        .battery_level = {.obj_id = ZMK_BTHOME_OBJECT_ID_BATTERY, .data = 0},
        .battery_voltage = {.obj_id = ZMK_BTHOME_OBJECT_ID_VOLTAGE_THOUSANDTH, .data = 0},
        .buttons = {[0 ...(BTHOME_BUTTON_NUM - 1)] = {.obj_id = ZMK_BTHOME_OBJECT_ID_BUTTON, .data = BTHOME_BTN_NONE}},
    },
};

#if IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)

struct zmk_bthome_encrypted_payload
{
    const uint8_t uuid[2];
    const uint8_t device_info;
    uint8_t encrypted_data[PAYLOAD_CONTENT_SIZE];
    // little endian counter
    uint32_t counter;
    // Message Integrity Check (4 bytes)
    uint8_t mic[BTHOME_ENCRYPT_TAG_LEN];
} __packed;

static union
{
    struct zmk_bthome_encrypted_payload data;
    uint8_t bytes[sizeof(struct zmk_bthome_encrypted_payload)];
} bthome_encrypted_payload = {
    .data = {
        .uuid = {BT_UUID_16_ENCODE(ZMK_BTHOME_SERVICE_UUID)},
        .device_info = ZMK_BTHOME_DEVICE_INFO,
    },
};

#endif // IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)

#if IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)
#define ACTIVE_BTHOME_PAYLOAD bthome_encrypted_payload
#else
#define ACTIVE_BTHOME_PAYLOAD bthome_payload
#endif

static const struct bt_data zmk_bthome_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    // TOOD device name
    BT_DATA(BT_DATA_SVC_DATA16, ACTIVE_BTHOME_PAYLOAD.bytes, sizeof(ACTIVE_BTHOME_PAYLOAD)),
};

struct zmk_bthome_button_event
{
    uint8_t index;
    uint8_t code;
} __packed;

#define BTHOME_BUTTON_QUEUE_SIZE 16
K_MSGQ_DEFINE(zmkbthome_button_msgq, sizeof(struct zmk_bthome_button_event), BTHOME_BUTTON_QUEUE_SIZE, 4);
static void zmkbthome_button_queue_work_handler(struct k_work *work);
K_WORK_DEFINE(zmkhome_button_queue, zmkbthome_button_queue_work_handler);
static void zmkbthome_adv_sent(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info);

static struct bt_le_ext_adv *bthome_adv;
static bool bthome_adv_active;

static const struct bt_le_ext_adv_cb bthome_adv_cb = {
    .sent = zmkbthome_adv_sent,
};

static void zmkbthome_button_queue_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (bthome_adv_active)
    {
        return;
    }

    struct zmk_bthome_button_event evt;
    if (k_msgq_get(&zmkbthome_button_msgq, &evt, K_NO_WAIT) != 0)
    {
        return;
    }

    LOG_INF("BTHome sending button event: index=%d code=0x%02x", evt.index, evt.code);

    // Not creating in SYS_INIT callback because bt_id is loaded after that
    // and bt is not ready yet at that time. bt_le_ext_adv_create will return -EAGAIN.
    if (bthome_adv == NULL)
    {
        if (!bt_is_ready())
        {
            LOG_WRN("Bluetooth not ready; deferring BTHome adv setup");
            // message is intentionally dropped, so we don't send a burst of ads later
            return;
        }

        int rc_create = bt_le_ext_adv_create(BT_LE_ADV_NCONN_IDENTITY, &bthome_adv_cb, &bthome_adv);
        if (rc_create != 0 || bthome_adv == NULL)
        {
            LOG_ERR("Failed to create BTHome advertiser in work: %d", rc_create);
            return;
        }

        LOG_INF("BTHome advertiser created in work");

#if IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)
        {
            static bt_addr_le_t id_addrs[CONFIG_BT_ID_MAX];
            size_t count = 1;
            bt_id_get(id_addrs, &count);
            if (count > 0)
            {
                uint8_t addr[6];
                memcpy(addr, id_addrs[BT_ID_DEFAULT].a.val, 6);
                zmk_bthome_encrypt_init(addr);
                LOG_INF("BTHome encryption initialized with BT ID address");
            }
            else
            {
                LOG_ERR("No BT ID address available for BTHome encryption");
            }

            /* Initialize random replay counter */
        }
        {
            int rc = sys_csrand_get(&bthome_encrypted_payload.data.counter,
                                    sizeof(bthome_encrypted_payload.data.counter));
            if (rc != 0)
            {
                LOG_ERR("Failed to initialize BTHome encryption replay counter: %d", rc);
                bthome_encrypted_payload.data.counter = sys_rand32_get(); // using non-crypto RNG as fallback
            }
            else
            {
                LOG_INF("BTHome encryption replay counter initialized");
            }
        }
#endif
    }

    // Clear all buttons, then set only the one from the event
    for (int i = 0; i < BTHOME_BUTTON_NUM; i++)
    {
        bthome_payload.data.buttons[i].data = BTHOME_BTN_NONE;
    }

    // BTHOME_BTN_NONE come from battery updates,
    // buttons array could have 0 elements if no buttons are configured.
    if (evt.code != BTHOME_BTN_NONE)
    {
        bthome_payload.data.buttons[evt.index].data = evt.code;
    }

    bthome_payload.data.packet_id.data++;

#if IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)
    bthome_encrypted_payload.data.counter =
        sys_cpu_to_le32(sys_le32_to_cpu(bthome_encrypted_payload.data.counter) + 1);

    int enc_rc = zmk_bthome_encrypt_payload(
        (bthome_payload.bytes + PAYLOAD_CONTENT_OFFSET), PAYLOAD_CONTENT_SIZE,
        bthome_encrypted_payload.data.counter,
        bthome_encrypted_payload.data.encrypted_data,
        bthome_encrypted_payload.data.mic);
    if (enc_rc != 0)
    {
        LOG_ERR("BTHome payload encryption failed: %d", enc_rc);
        return;
    }
#endif

    int rc = bt_le_ext_adv_set_data(bthome_adv, zmk_bthome_ad, ARRAY_SIZE(zmk_bthome_ad), NULL, 0);
    if (rc != 0)
    {
        LOG_ERR("Failed to set BTHome advertisement data: %d", rc);
        return;
    }

    rc = bt_le_ext_adv_start(bthome_adv, BT_LE_EXT_ADV_START_PARAM(CONFIG_ZMK_BTHOME_ADV_TIMEOUT, CONFIG_ZMK_BTHOME_ADV_PACKETS));
    if (rc == 0)
    {
        bthome_adv_active = true;
    }
    else
    {
        LOG_ERR("Failed to start BTHome advertisement: %d", rc);
    }
}

static bool bthome_button_code_valid(const uint8_t button_code)
{
    switch (button_code)
    {
    case BTHOME_BTN_NONE:
    case BTHOME_BTN_PRESS:
    case BTHOME_BTN_DOUBLE_PRESS:
    case BTHOME_BTN_TRIPLE_PRESS:
    case BTHOME_BTN_LONG_PRESS:
    case BTHOME_BTN_LONG_DOUBLE_PRESS:
    case BTHOME_BTN_LONG_TRIPLE_PRESS:
    case BTHOME_BTN_HOLD_PRESS:
        return true;
    default:
        return false;
    }
}

int zmk_bthome_queue_button_event(uint8_t index, uint8_t button_code)
{
    if (index >= BTHOME_BUTTON_NUM)
    {
        return -EINVAL;
    }

    if (!bthome_button_code_valid(button_code))
    {
        return -EINVAL;
    }
    struct zmk_bthome_button_event evt = {
        .index = index,
        .code = button_code,
    };

    int rc = k_msgq_put(&zmkbthome_button_msgq, &evt, K_NO_WAIT);
    if (rc != 0)
    {
        if (rc != -EAGAIN)
        {
            return rc;
        }

        // Drop oldest to make room for newest, then retry
        struct zmk_bthome_button_event drop;
        k_msgq_get(&zmkbthome_button_msgq, &drop, K_NO_WAIT);
        rc = k_msgq_put(&zmkbthome_button_msgq, &evt, K_NO_WAIT);
        if (rc != 0)
        {
            return rc;
        }
    }

    LOG_DBG("Queued BTHome button event: index=%d code=0x%02x", evt.index, evt.code);

    k_work_submit(&zmkhome_button_queue);
    return 0;
}

static void zmkbthome_adv_sent(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info)
{
    ARG_UNUSED(adv);
    ARG_UNUSED(info);

    bthome_adv_active = false;
    k_work_submit(&zmkhome_button_queue);
}

#if IS_ENABLED(CONFIG_ZMK_BTHOME_BATTERY_LEVEL)

#if IS_ENABLED(CONFIG_ZMK_BTHOME_BATTERY_VOLTAGE) && DT_HAS_CHOSEN(zmk_battery)
// battery for reading voltage
static const struct device *const battery = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
static void bthome_batt_work_handler(struct k_work *work);
K_WORK_DEFINE(bthome_batt_work, bthome_batt_work_handler);

int update_bthome_battery_voltage()
{
    int rc;
    rc = sensor_sample_fetch_chan(battery, SENSOR_CHAN_GAUGE_VOLTAGE);
    if (rc != 0)
    {
        LOG_DBG("Failed to fetch battery values: %d", rc);
        return rc;
    }

    struct sensor_value voltage;
    rc = sensor_channel_get(battery, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage);

    if (rc != 0)
    {
        LOG_DBG("Failed to get battery voltage: %d", rc);
        return rc;
    }

    uint16_t mv = voltage.val1 * 1000 + (voltage.val2 / 1000);

    bthome_payload.data.battery_voltage.data = sys_cpu_to_le16(mv);
    return 0;
}

static void bthome_batt_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int rc = update_bthome_battery_voltage();
    if (rc != 0)
    {
        LOG_ERR("BTHome battery voltage update failed: %d", rc);
        // trigger an advertisement anyway to since battery level may have changed
    }

    // Push an advertisement with updated battery data
    zmk_bthome_queue_button_event(0, BTHOME_BTN_NONE);
}
#endif

int batt_state_changed_listener(const zmk_event_t *eh)
{
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev == NULL)
    {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // set battery level to the payload
    uint8_t battery_level = ev->state_of_charge;
    bthome_payload.data.battery_level.data = battery_level;

#if IS_ENABLED(CONFIG_ZMK_BTHOME_BATTERY_VOLTAGE) && DT_HAS_CHOSEN(zmk_battery)
    // Read voltage from sensor asynchronously (if enabled).
    // If we have voltage reading enabled, delay sending
    // the advertisement until we have the voltage too.
    k_work_submit(&bthome_batt_work);
#else
    // Queue a "no button" update to push battery change
    zmk_bthome_queue_button_event(0, BTHOME_BTN_NONE);
#endif

    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(battery_changed, batt_state_changed_listener);
ZMK_SUBSCRIPTION(battery_changed, zmk_battery_state_changed);

#endif // CONFIG_ZMK_BTHOME_BATTERY_LEVEL
