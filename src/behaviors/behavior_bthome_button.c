/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_bthome_button

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#include <stdint.h>

#include <zmk_bthome/zmk_bthome.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_bthome_button_config
{
    // The index of the button
    uint8_t index;
};

static int on_bthome_button_binding_pressed(struct zmk_behavior_binding *binding,
                                            struct zmk_behavior_binding_event event)
{
    ARG_UNUSED(event);

    const struct behavior_bthome_button_config *cfg = zmk_behavior_get_binding(binding->behavior_dev)->config;

    zmk_bthome_queue_button_event(cfg->index, (uint8_t)binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_bthome_button_binding_released(struct zmk_behavior_binding *binding,
                                             struct zmk_behavior_binding_event event)
{
    ARG_UNUSED(event);
    ARG_UNUSED(binding);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api bthome_button_driver_api = {
    .binding_pressed = on_bthome_button_binding_pressed,
    .binding_released = on_bthome_button_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
};

#define BTHOME_BUTTON_INST(n)                                                               \
    static const struct behavior_bthome_button_config behavior_bthome_button_config_##n = { \
        .index = n,                                                                         \
    };                                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n,                                                              \
                            NULL,                                                           \
                            NULL,                                                           \
                            NULL,                                                           \
                            &behavior_bthome_button_config_##n,                             \
                            POST_KERNEL,                                                    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                            \
                            &bthome_button_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BTHOME_BUTTON_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
