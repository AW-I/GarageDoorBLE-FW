# GarageDoorBLE: Forcedoor Garage Door Controller

ESP32-C3 firmware that replaces the Forcedoor Wi-Fi dongle. It plugs into the motor unit's
accessory port and lets you control the door two ways:

- **Bluetooth, from the phone app.** Works with no network at all. The app is also how you set up
  Wi-Fi, Home Assistant, the device name and the door password.
- **Home Assistant over Wi-Fi.** MQTT with auto-discovery: the door shows up as a garage cover with
  live position, plus the motor light and a few sensors.

Phone control is protected by a door password, and Home Assistant control by the MQTT broker login.

```
Phone app ──BLE──> ble_gdp ──> app_ble_cmd ─┐
                                           ├──> app_door ──> motor ──UART──> Forcedoor motor
Home Assistant ──MQTT──> ha_mqtt ──────────┘        │
                                                    └──> state ──> BLE notify + MQTT publish
```

## Features

- Open, close, stop, toggle (wall-button behaviour) and motor light on/off
- Live door position (%) and movement state, pushed to the app and to Home Assistant
- Works offline over BLE. Wi-Fi and Home Assistant are optional
- Firmware updates from the phone app over BLE, with automatic rollback if a new image fails
- Status LED for Wi-Fi and Home Assistant connection state

## Hardware

| Part | Notes |
|---|---|
| **ESP32-C3 SuperMini** | Any ESP32-C3 board works; the defaults match the SuperMini |
| USB-A plug or cable | Plugs into the motor's accessory socket |
| 24 V → 5 V buck converter | Only if your motor's VBUS is 24 V |
| 5 kΩ and 10 kΩ resistors | Only if your motor's data lines are 5 V: divider that brings the motor's TX down to 3.3 V |

### Wiring

The motor's USB-A socket is **not USB**. It carries power and a UART, and the voltages differ
between motor units:

| Motor | VBUS | D- / D+ | Extra parts |
|---|---|---|---|
| **Steel-Line (tested)** | **5 V** | **3.3 V** | None. Wire straight to the board |
| Other units (e.g. the ESPHome Forcedoor reference) | **24 V** | **5 V** | Buck converter on VBUS, divider on D- |

> [!IMPORTANT]
> **Measure before connecting anything.** The voltages aren't marked on the motor. With the motor
> powered, use a multimeter to measure:
>
> 1. **VBUS to GND.** About 5 V can go straight to the SuperMini **5V** pin. About 24 V must go
>    through a buck converter. 24 V on the 5V pin destroys the board.
> 2. **D- to GND.** The line idles high, so it reads close to its logic level. About 3.3 V can go
>    straight to GPIO0. About 5 V needs the 5k/10k divider, because the ESP32-C3 is not 5 V tolerant.
>    Don't add the divider to a 3.3 V line: it drops the signal to about 2.2 V, which may not read
>    reliably.
>
> D+ connects straight to GPIO1 either way, because the ESP drives it at 3.3 V.

| Motor USB-A pin | Signal | Connect to |
|---|---|---|
| VBUS | Power (5 V or 24 V) | SuperMini **5V** pin, through a buck converter if 24 V |
| D- | Motor TX | **GPIO0** (ESP RX), through a 5k/10k divider if 5 V |
| D+ | Motor RX | **GPIO1** (ESP TX) |
| GND | Ground | **GND** |

Steel-Line (5 V VBUS, 3.3 V data):
```
                    ESP32-C3 SuperMini
motor D-  ──────────── GPIO0  (RX)
motor D+  ──────────── GPIO1  (TX)
motor GND ──────────── GND
motor 5V  ──────────── 5V
```

24 V VBUS, 5 V data:
```
                    ESP32-C3 SuperMini
motor D- ──[5k]──┬──── GPIO0  (RX)
                 │
               [10k]
                 │
motor GND ───────┴──── GND
motor D+ ───────────── GPIO1  (TX)
motor 24V ── buck ──── 5V
```

UART is 115200 8N1. The pins can be changed in `idf.py menuconfig` → *Garage Door Controller →
Motor UART*. GPIO0 and GPIO1 are free pins on the SuperMini. If you move them, avoid strapping pins
2/8/9 and the USB pins 18/19.

Some SuperMini boards have a poorly matched antenna and can't join Wi-Fi at full power. If the
access point stops answering during authentication three times in a row, the controller retries at
8.5 dBm. The log shows `retrying at reduced TX power`.

### Status LED

The SuperMini's onboard blue LED (GPIO8, active low):

| LED | Meaning |
|---|---|
| Off | No Wi-Fi configured |
| Blinking (2 Hz) | Searching for or reconnecting to Wi-Fi |
| Double blink (every 1.2 s) | Wi-Fi connected, MQTT broker configured but not connected |
| Solid | Connected to Wi-Fi, and to the broker if one is configured |
| Short flash (inverted) | A command was sent to the motor |

## Build and flash

Requires [ESP-IDF v6.1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/).

Firmware images are signed (see [Signed firmware](#signed-firmware)), so generate your own signing
key before the first build:

```bash
idf.py secure-generate-signing-key --version 2 --scheme rsa3072 secure_boot_signing_key.pem
```
```bash
idf.py set-target esp32c3
```
```bash
idf.py build flash monitor
```

`espressif/mqtt` is downloaded from the component registry on the first build. Plug the SuperMini
in over USB-C to flash it. If it isn't detected, hold BOOT while plugging it in.

The first USB flash writes the bootloader and the OTA partition table. After that, updates can go
over BLE from the app (see [Firmware updates](#firmware-updates-over-ble)).

## First-time setup

1. Power the controller and open the phone app. It finds the controller as **BLEDoor**.
2. The default door password is `default`. Change it in the app under Settings → Password.
3. Optional: enter Wi-Fi and Home Assistant (MQTT) details in Settings and save.

The defaults can be changed in `menuconfig` → *Garage Door Controller → Defaults*.

## Home Assistant

1. Install the Mosquitto broker add-on and the MQTT integration, and create an HA user for the
   controller.
2. In the app, enter the broker as `mqtt://<home-assistant-ip>:1883` with that user and password,
   then save.
3. The controller appears under **Settings → Devices & services → MQTT** with its device name.

| Entity | Type |
|---|---|
| Garage door (open / close / stop, position) | Cover |
| LED (motor light) | Light |
| Restart | Button |
| Position, Operation, Wi-Fi signal | Sensors |

Availability uses a last-will message on `garage_door/<node_id>/status`. The node id is `gdc_`
followed by the last 3 bytes of the MAC. Topics are
`garage_door/<node_id>/door/set|state|position|operation`, `led/set|state`, `restart/press` and
`rssi`.

## Firmware updates over BLE

The phone app carries a copy of the firmware and updates the controller over Bluetooth. No server
is involved.

The flash has two 1.875 MB app slots (`partitions.csv`). The new firmware is written to the slot
that isn't running. After a restart it runs in trial mode, and the bootloader keeps it once either:

- a phone logs in over BLE (the app does this straight after the update), or
- it has run for `GDC_OTA_CONFIRM_AFTER_S` (default 10 minutes).

If it crashes or resets before then, the bootloader goes back to the previous firmware. Before
switching, `esp_ota_end` checks the image's signature, SHA-256 and chip type, and the controller
refuses images built from a different project.

### Signed firmware

Every build is signed with `secure_boot_signing_key.pem` (RSA-3072), and a BLE update is rejected
unless it is signed with the same key as the firmware already running. Someone who knows the door
password still can't install their own firmware over Bluetooth.

- **The key is not in git** (`*.pem` is ignored). Generate your own with the command under
  [Build and flash](#build-and-flash).
- **Back the key up.** A controller only accepts updates signed with the key its current firmware
  was built with. If you lose the key, every controller needs a USB flash to take a new one.
- **Never publish the key.** Anyone with it can sign firmware your controllers will accept.
- **The first signed image installs over BLE.** Firmware built before signing was enabled doesn't
  check signatures, so it accepts the first signed update. From then on only signed images are
  accepted.
- **USB flashing still works** with any image. This uses `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`,
  which protects remote (Bluetooth) updates without burning eFuses. Hardware Secure Boot, which also
  locks down USB flashing, is a separate and **irreversible** step that this project doesn't
  enable.

Check a build's signature with:

```bash
python -m espsecure verify-signature --version 2 --keyfile secure_boot_signing_key.pem build/GarageDoorBLE.bin
```

**Releasing an update:**

1. Bump `VERSION` in `main/version.h` and run `idf.py build`. CMake embeds the version in the
   image, where the app reads it.
2. In the app project, run `gradlew copyFirmware`. This copies `build/GarageDoorBLE.bin` into the
   app's assets.
3. Build the app. Settings → Firmware offers the update when the bundled version is newer than the
   controller's.

**Upgrading from 1.0.0:** 1.0.0 used a single-app partition table, so flash 1.1.0 or later over USB
once. Settings and paired phones are kept because the `nvs` partition did not move.

## Configuration

`idf.py menuconfig` → *Garage Door Controller*:

| Menu | Options |
|---|---|
| Motor UART | UART port, TX/RX GPIO (default 1/0), baud, command gap, retries, echo timeout |
| Status LED | GPIO (default 8, -1 for none), active low |
| Defaults | BLE device name (`BLEDoor`), door password (`default`) |
| Home Assistant | Discovery prefix (`homeassistant`), base topic (`garage_door`) |
| Supervisor | Idle BLE disconnect (300 s), periodic reboot (24 h), OTA confirm time (600 s) |

## Project layout

| Module | Path | Role |
|---|---|---|
| `settings` | `components/settings` | NVS-backed config (name, door password, Wi-Fi, MQTT). Constant-time password check. |
| `motor` | `components/motor` | Forcedoor UART protocol (`motor_proto.c`, pure codec) and the UART driver task (`motor.c`). |
| `ble_gdp` | `components/ble_gdp` | NimBLE GATT server and advertising. Transport only. |
| `net_wifi` | `components/net_wifi` | Wi-Fi station with exponential back-off reconnect. |
| `ha_mqtt` | `components/ha_mqtt` | MQTT client, Home Assistant discovery, LWT availability, commands. |
| `status_led` | `components/status_led` | Blink patterns for a plain LED. |
| app | `main/` | `app_door` (actions and state fan-out), `app_ble_cmd` (BLE command protocol), `app_net` (applies settings), `app_ota` (BLE firmware update), `app_supervisor` (watchdog, idle disconnect, periodic reboot). |

The components don't depend on each other; only `main/` wires them together.

## Protocol reference

### Motor UART

Frames are `[hdr][len][payload][sum][0x0D]`, where `len` is the whole frame length and `sum` is the
sum of all preceding bytes & 0xFF.

| Command | Frame |
|---|---|
| Open | `23 07 41 00 00 6B 0D` |
| Stop | `23 07 41 01 00 6C 0D` |
| Close | `23 07 41 02 00 6D 0D` |
| LED on / off | `23 07 41 F0 00 5B 0D` / `23 07 41 F1 00 5C 0D` |

Status frames are 17 bytes: `40 11 49 <op> <led> … <pos%> … <sum> 0D`. `op` is 0 opening,
1 stopped, 2 closing. `led` is F0 on, F1 off. The motor echoes each accepted command as a
`40 07 41 <cmd> …` frame.

### BLE (phone app)

The controller advertises the service UUID, with its name in the scan response. The phone starts
pairing (bonding, LE Secure Connections, Just Works).

| Characteristic | Props | Content |
|---|---|---|
| Service `713D0000-503E-4C75-BA94-3148F18D941E` | | |
| Command `713D0003-…` | write, write-no-rsp, read | Write `[cmd][payload]`, then read `[status][data]` |
| State `713D0002-…` | read, notify | 4 bytes: position % (FF unknown), op (0 opening / 1 stopped / 2 closing / FF), LED (1/0/FF), flags (bit0 Wi-Fi configured, bit1 Wi-Fi up, bit2 MQTT up) |
| Data `713D0004-…` | write, write-no-rsp | Firmware image chunks `<offset u32 LE><bytes>`, only during an update |

Every command except AUTH needs a successful AUTH on the current connection.

| Cmd | Payload | Action |
|---|---|---|
| `0x01` AUTH | password | Unlocks the other commands until disconnect |
| `0x02` DOOR | 1 byte action | 0 open, 1 stop, 2 close, 4 LED on, 5 LED off, 6 toggle (stop if moving, else open/close) |
| `0x03` SET | key, flags, value | flags bit0 = first chunk, bit1 = last chunk (`0x03` for a single write) |
| `0x04` GET | key | data: value. Secrets come back as `********` when set |
| `0x05` APPLY | – | Reconnect Wi-Fi and MQTT with the saved settings |
| `0x06` INFO | – | data: `fw=..;name=..;wifi=..;ssid=..;ip=..;rssi=..;mqtt=..;pos=..;op=..;led=..` |
| `0x07` REBOOT | – | Restart the controller |
| `0x08` FACTORY RESET | – | Erase settings and paired phones, then reboot |
| `0x09` OTA_BEGIN | size u32 LE | Open the inactive slot. `0x02` if the image doesn't fit |
| `0x0A` OTA_STATUS | – | data: next expected offset (u32 LE). `0x03` once a flash write failed |
| `0x0B` OTA_END | – | Verify the image, select it for boot, reboot |
| `0x0C` OTA_ABORT | – | Drop the update (disconnecting does the same) |

Reply status: `0x00` OK, `0x01` auth (wrong password or not logged in), `0x02` invalid payload,
`0x03` failed, `0x04` unknown command. **A wrong password drops the link after 1 s.**

Setting keys: 1 Wi-Fi SSID, 2 Wi-Fi password (empty or 8–64), 3 MQTT URI (`mqtt://…` or
`mqtts://…`), 4 MQTT user, 5 MQTT password, 6 device name (≤ 20), 7 door password (≤ 20).

During an update the phone sends windows of 32 chunks as write-without-response on the data
characteristic, then asks OTA_STATUS. The controller drops any chunk whose offset isn't the next
expected byte, and the phone resends from the offset OTA_STATUS returns.

Example provisioning sequence:
```
01 <pw>                        AUTH
03 01 03 "MyWiFi"              SET SSID
03 02 03 "wifipassword"        SET Wi-Fi password
03 03 03 "mqtt://10.0.0.5:1883"
03 04 03 "garage"              MQTT user
03 05 03 "secret"              MQTT password
05                             APPLY
06                             INFO -> check wifi=Connected;mqtt=1
```
