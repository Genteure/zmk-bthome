# ZMK BTHome Module

ZMK BTHome is a [ZMK module](https://zmk.dev/docs/features/modules) that adds support for sending sensor data and button events over Bluetooth Low Energy (BLE) using the BTHome protocol.

[ZMK](https://zmk.dev/) is an open source, wireless-first keyboard firmware based on Zephyr RTOS. It's modular and composable, making it easy to extend functionality through modules like this one.

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
CONFIG_BT_EXT_ADV_MAX_ADV_SET=2
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

```devicetree
#include <dt-bindings/zmk_bthome/button.h>

/ {
    behaviors {
        bthome0: bthome_button0 { // name can be anything, but ORDER matters
            compatible = "zmk,behavior-bthome-button";
            #binding-cells = <1>;
            display-name = "BTHome Button 1"; // Optional, shown in ZMK Studio
        };
        bthome1: bthome_button1 {
            compatible = "zmk,behavior-bthome-button";
            #binding-cells = <1>;
            display-name = "BTHome Button 2";
        };
        bthome2: bthome_button2 {
            compatible = "zmk,behavior-bthome-button";
            #binding-cells = <1>;
            display-name = "BTHome Button 3";
        };
        // As many as you want...
    };

    keymap {
        compatible = "zmk,keymap";
        default_layer {
            bindings = <&bthome0 BTHOME_BTN_PRESS &bthome1 BTHOME_BTN_TRIPLE_PRESS ...>;
        };
    };
};
```

IMPORTANT: The button index is determined by the order in which the behaviors are defined in your keymap, meaning if you have `bthome_a`, `bthome_b`, "A" will be button 1 and "B" will be button 2. If you have `bthome_b` defined before `bthome_a`, then "B" will be button 1 and "A" will be button 2.

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

TODO support sending from the source side?

### Device Name

TODO

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
