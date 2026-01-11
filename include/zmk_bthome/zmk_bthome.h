/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

// BTHome device information, from https://bthome.io/format/
#define ZMK_BTHOME_ENCRYPTION_FLAG 0x01
#define ZMK_BTHOME_TRIGGER_BASED_FLAG 0x04 // irregular advertising interval
#define ZMK_BTHOME_VERSION_2 0x40

#define ZMK_BTHOME_OBJECT_ID_PACKET_ID 0x00
#define ZMK_BTHOME_OBJECT_ID_BATTERY 0x01
#define ZMK_BTHOME_OBJECT_ID_VOLTAGE_THOUSANDTH 0x0C
#define ZMK_BTHOME_OBJECT_ID_VOLTAGE_TENTH 0x4A
#define ZMK_BTHOME_OBJECT_ID_CONNECTIVITY 0x19
#define ZMK_BTHOME_OBJECT_ID_BUTTON 0x3A
#define ZMK_BTHOME_OBJECT_ID_DIMMER 0x3C

#define ZMK_BTHOME_SERVICE_UUID 0xfcd2
#define ZMK_BTHOME_SERVICE_UUID_1 0xd2
#define ZMK_BTHOME_SERVICE_UUID_2 0xfc

// Device info includes encryption flag when configured
#if IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)
#define ZMK_BTHOME_DEVICE_INFO \
    (ZMK_BTHOME_VERSION_2 | ZMK_BTHOME_TRIGGER_BASED_FLAG | ZMK_BTHOME_ENCRYPTION_FLAG)
#else
#define ZMK_BTHOME_DEVICE_INFO (ZMK_BTHOME_VERSION_2 | ZMK_BTHOME_TRIGGER_BASED_FLAG)
#endif

#define BTHOME_ENCRYPT_TAG_LEN 4

#if IS_ENABLED(CONFIG_ZMK_BTHOME_ENCRYPTION)
void zmk_bthome_encrypt_init(const uint8_t ble_addr[6]);
int zmk_bthome_encrypt_payload(const uint8_t *plaintext, const size_t plaintext_len,
                               const uint32_t replay_counter, uint8_t *enc_out,
                               uint8_t mic_out[BTHOME_ENCRYPT_TAG_LEN]);
#endif

int zmk_bthome_queue_button_event(uint8_t index, uint8_t button_code);
