# ZMK BTHome Module

This [ZMK module](https://zmk.dev/docs/features/modules) adds support sending BTHome sensor data over Bluetooth Low Energy (BLE) advertisements.

[ZMK](https://zmk.dev/) is an open source, wireless-first keyboard firmware based on Zephyr RTOS.

[BTHome](https://bthome.io/) is an open standard for transmitting sensor data and events over BLE, commonly used in smart home devices. [Home Assistant](https://www.home-assistant.io/integrations/bthome/) supports BTHome natively out of the box.

## Features

This module allows ZMK-powered keyboards to send the following data:

- Battery Level (percentage) and Battery Voltage
- Buttons (0, 1 or more, with all press types)

## Installation

Add this module to your `config/west.yml`, see <https://zmk.dev/docs/features/modules#building-with-modules> for more details.

In your keyboard's `.conf` file, enable the module:

```kconfig
CONFIG_ZMK_BTHOME=y
```

## Configuration

### Battery

Battery level and battery voltage reporting are enabled by default. To disable them, add the following to your keyboard's `.conf` file:

```kconfig
# Disable battery level reporting
CONFIG_ZMK_BTHOME_BATTERY_LEVEL=n
# Disable battery voltage reporting
CONFIG_ZMK_BTHOME_BATTERY_VOLTAGE=n
```

For split keyboards, each keyboard part will independently report its own battery level and voltage.

### Buttons

Buttons are ZMK behaviors. Add any number of bthome button behaviors to your keymap.

```dts
/ {
    behaviors {
        bthome0: bthome_button0 {
            compatible = "zmk,behavior-bthome-button";
            #binding-cells = <1>;
            display-name = "BTHome Button 0";
        };
        bthome1: bthome_button1 {
            compatible = "zmk,behavior-bthome-button";
            #binding-cells = <1>;
            display-name = "BTHome Button 1";
        };
        // As many as you want...
    };
};
```

Then, reference them in your keymap:

```dts
#include <dt-bindings/zmk_bthome/button.h>

...
    default_layer {
        bindings = <&bthome0 BTHOME_BTN_PRESS &bthome1 BTHOME_BTN_TRIPLE_PRESS ...>;
    };
...
```

Possible button press types are:

- `BTHOME_BTN_PRESS`
- `BTHOME_BTN_DOUBLE_PRESS`
- `BTHOME_BTN_TRIPLE_PRESS`
- `BTHOME_BTN_LONG_PRESS`
- `BTHOME_BTN_LONG_DOUBLE_PRESS`
- `BTHOME_BTN_LONG_TRIPLE_PRESS`
- `BTHOME_BTN_HOLD_PRESS`

Compose with ZMK built-in behaviors like hold-tap and tap-dance to create real "multi-function" buttons, or simply put them on different keys and layers.

For split keyboards, button presses will be reported from the central side. After any changes on behaviors, make sure to flash all keyboard parts.

Note the button index is determined by the order in which the behaviors are defined in your keymap, meaning if you have `bthome_a`, `bthome_b`, "A" will be button 0 and "B" will be button 1. If you have `bthome_b` defined before `bthome_a`, then "B" will be button 0 and "A" will be button 1.

### Device Name

By default, the BTHome device name will be the value of `CONFIG_ZMK_KEYBOARD_NAME`. If that is not set, it will default to `ZMK-XXXXXX`, where `XXXXXX` are the last 6 digits of the device's MAC address.

You can override the device name by setting `CONFIG_ZMK_BTHOME_DEVICE_NAME` in your keyboard's `.conf` file:

```kconfig
# Up to 15 characters
CONFIG_ZMK_BTHOME_DEVICE_NAME="My Keyboard Right"
```

### Encryption

By default, BTHome advertisements are unencrypted. You can enable encryption by setting `CONFIG_ZMK_BTHOME_ENCRYPTION_KEY` in your keyboard's `.conf` file:

```kconfig
# 16-byte (32 hex characters) encryption key
CONFIG_ZMK_BTHOME_ENCRYPTION_KEY="00112233445566778899AABBCCDDEEFF"
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Credits

TODO
