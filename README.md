# ZMK BTHome Module

ZMK BTHome is a [ZMK module](https://zmk.dev/docs/features/modules) that adds support for sending sensor data and button events over Bluetooth Low Energy (BLE) using the BTHome protocol.

[ZMK](https://zmk.dev/) is an open source, wireless-first keyboard firmware based on Zephyr RTOS. It's modular and composable, making it easy to extend functionality through modules like this one.

[BTHome](https://bthome.io/) is an open standard for transmitting sensor data and events over BLE, commonly used in smart home devices. [Home Assistant](https://www.home-assistant.io/integrations/bthome/) supports BTHome natively out of the box.

## Features

- BLE BTHome v2 advertisements
- Optional encryption
- Battery Level (percentage) and Battery Voltage
- Configurable amount of buttons with all press types
- Optional custom device name

## Installation

Add this module to your `config/west.yml`:

```yaml
manifest:
  remotes:
    # ... existing remotes ...
    - name: genteure
      url-base: https://github.com/genteure
  projects:
    # ... existing projects ...
    - name: zmk-bthome
      remote: genteure
      revision: main
  # ... other manifest entries ...
```

See <https://zmk.dev/docs/features/modules#building-with-modules> for more information on adding modules to your ZMK build.

Then in your keyboard's `.conf` file (`config/keyboard_name.conf`), enable ZMK BTHome:

```kconfig
CONFIG_ZMK_BTHOME=y
CONFIG_BT_EXT_ADV_MAX_ADV_SET=2
```

This module builds with ZMK `v0.3` and the to-be-0.4 `main` branch. Tested working on an actual keyboard with ZMK `main` branch as of January 2026.

## Configuration

### Device Name

By default, the BTHome device name will be the value of `CONFIG_ZMK_KEYBOARD_NAME`, or "ZMK" if keyboard name is not set.

You can override the device name by setting `CONFIG_ZMK_BTHOME_DEVICE_NAME` in your keyboard's `.conf` file:

```kconfig
CONFIG_ZMK_BTHOME_DEVICE_NAME="A"
```

Setting `CONFIG_ZMK_BTHOME_DEVICE_NAME=""` (empty string) will remove the device name entry from the advertisement entirely, leaving more space for BTHome sensor data. Removing the device name does not affect how Home Assistant identifies the device and is highly recommended if encryption is enabled. See Size Limitations section below for details.

Home Assistant by default will use the device name + last 4 characters of the MAC address as the display name, or "BTHome sensor XXXX" if the device name is empty. You can always change the display name in Home Assistant so setting a custom device name here is not strictly necessary.

### Buttons

Buttons are ZMK behaviors. Add any number of bthome button behaviors to your keymap.

```devicetree
#include <dt-bindings/zmk_bthome/button.h>

/ {
    behaviors {
        // name can be anything, but ORDER matters
        bthome0: bthome_button0 {
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

```devicetree
// https://zmk.dev/docs/keymaps/behaviors/hold-tap
/ {
    behaviors {
        htbth0: hold_tap_bthome0 {
            compatible = "zmk,behavior-hold-tap";
            #binding-cells = <2>;
            flavor = "hold-preferred";
            tapping-term-ms = <200>;
            bindings = <&bthome0>, <&bthome0>;
            display-name = "Hold-Tap BTHome Button 1";
        };
    };

    keymap {
        compatible = "zmk,keymap";
        default_layer {
            // Hold sends long press, tap sends press
            bindings = <&htbth0 BTHOME_BTN_LONG_PRESS BTHOME_BTN_PRESS>;
        };
    };
};
```

```devicetree
// https://zmk.dev/docs/keymaps/behaviors/tap-dance
/ {
    behaviors {
        tdbth0: tap_dance_bthome0 {
            compatible = "zmk,behavior-tap-dance";
            #binding-cells = <0>;
            tapping-term-ms = <200>;
            bindings = <&bthome0 BTHOME_BTN_PRESS>, <&bthome0 BTHOME_BTN_DOUBLE_PRESS>, <&bthome0 BTHOME_BTN_TRIPLE_PRESS>;
        };
    };

    keymap {
        compatible = "zmk,keymap";

        default_layer {
            // Single, double, and triple press
            bindings = <&tdbth0>;
        };
    };
};
```

For split keyboards, button presses will be reported from the central side only, since keymap processing happens there.

TODO support sending from the source side?

### Battery

Battery level and battery voltage reporting are enabled by default. To disable them, add the following to your keyboard's `.conf` file:

```kconfig
# Disable battery level reporting
CONFIG_ZMK_BTHOME_BATTERY_LEVEL=n
# Disable battery voltage reporting
CONFIG_ZMK_BTHOME_BATTERY_VOLTAGE=n
```

For split keyboards, each keyboard part will independently report its own battery level and voltage.

### Encryption

By default, BTHome advertisements are unencrypted. You can enable encryption by setting the following options in your keyboard's `.conf` file:

```kconfig
# 16-byte (32 hex characters) encryption key
CONFIG_ZMK_BTHOME_ENCRYPTION_KEY="00112233445566778899AABBCCDDEEFF"
```

You can use the same encryption key across multiple keyboard parts. Home Assistant will prompt you to enter the bind key (a.k.a. encryption key) in the Integrations/Devices page.

Enabling encryption takes up more space in the advertisement packet, see the Size Limitations section below.

As of Home Assistant 2026.1, you can't remove the bind key from an existing BTHome device. See <https://github.com/home-assistant/core/pull/159646>.

Encryption prevents observers from seeing your button presses and battery status, but doesn't fully prevent replay attacks. As of writing (January 2026), advertisements with encryption counter less than 100 are accepted even if they are less than the last received counter to allow device restarts, and the assumption is the counter will start from 0 on restart.

Here's a cryptographically secure one-liner to paste into your browser console:

```js
[...crypto.getRandomValues(new Uint8Array(16))].map(b => b.toString(16).padStart(2, '0')).join('')
```

### Size Limitations

BLE advertisement packets have a maximum size of 31 bytes. There are 3 bytes of BLE flags, another 7 bytes of BTHome related header, leaving just **21 bytes** for the payload.

Each type of data takes different amounts of space:

- Battery Level: 2 bytes
- Battery Voltage: 3 bytes
- Button: 2 bytes each

Encryption will take an additional 8 bytes if enabled.

If `CONFIG_ZMK_BTHOME_DEVICE_NAME` is set, that takes up an additional `sizeof(CONFIG_ZMK_BTHOME_DEVICE_NAME) + 2` bytes in the advertisement packet.

If the total data exceeds the size limit, build will fail with error `BTHome advertisement payload exceeds maximum advertisement size`.

#### Example 1

- 0 bytes: `CONFIG_ZMK_BTHOME_DEVICE_NAME=""` (empty string)
- 2 bytes: Battery Level
- 3 bytes: Battery Voltage
- 8 bytes: 4 buttons
- 8 bytes: Encryption overhead
- Total: **21** bytes

#### Example 2

- 5 bytes: `CONFIG_ZMK_BTHOME_DEVICE_NAME="ZMK"` (3 characters + 2 bytes header)
- 2 bytes: Battery Level
- 3 bytes: Battery Voltage
- 2 bytes: 1 button
- 0 bytes: No encryption
- Total: **12** bytes

#### Example 3

- 7 bytes: `CONFIG_ZMK_BTHOME_DEVICE_NAME="Corne"` (5 characters + 2 bytes header)
- 2 bytes: Battery Level
- 3 bytes: Battery Voltage
- 8 bytes: 4 buttons
- 0 bytes: No encryption
- Total: **20** bytes

#### Example 4

- 11 bytes: `CONFIG_ZMK_BTHOME_DEVICE_NAME="nice name"` (9 characters + 2 bytes header)
- 2 bytes: Battery Level
- 3 bytes: Battery Voltage
- 0 bytes: No buttons
- 8 bytes: Encryption overhead
- Total: **24** bytes -> Build fails

### Advertising Parameters

You can customize the advertising timeout and number of packets sent per interval by setting the following options in your keyboard's `.conf` file:

```kconfig
# Advertising timeout in units of 10 ms (default: 100, i.e., 1000 ms)
CONFIG_ZMK_BTHOME_ADV_TIMEOUT=100
# Number of advertising packets sent per interval (default: 10)
CONFIG_ZMK_BTHOME_ADV_PACKETS=10
```

By default, each BTHome event is advertised for up to 1 second (100 * 10 ms) or up to 10 times during that period, whichever comes first. Multiple BTHome events occurring right after each other will be queued and advertised sequentially.

You can adjust these values to balance between time spent advertising each BTHome event and reliability of receiving the advertisements. Too low values may result in missed events.

(To avoid confusion, we're using "events" to refer to BTHome/Home Assistant events and "packets" for what Zephyr calls "advertising events".)

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Credits

- <https://github.com/jeffrizzo/bthome-zephyr>
