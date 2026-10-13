# Garage Door Controller

ESP32-C3 firmware that replaces the Forcedoor Wi-Fi dongle. It talks to the motor unit over its
UART port and can be controlled two ways:

- **Bluetooth (NimBLE):** the phone app. Works with no network at all. It is also how you set up
  Wi-Fi, MQTT, the device name and the door password.
- **Home Assistant over Wi-Fi:** MQTT with auto-discovery.

Phone control is password protected; Home Assistant control is protected by the MQTT broker login.

```
Phone app ──BLE──> ble_gdp ──> app_ble_cmd ─┐
                                           ├──> app_door ──> motor ──UART──> Forcedoor motor
Home Assistant ──MQTT──> ha_mqtt ──────────┘        │
                                                    └──> state ──> BLE notify + MQTT publish
```

## Layout

| Module | Path | Role |
|---|---|---|
| `settings` | `components/settings` | NVS-backed config (name, door password, Wi-Fi, MQTT). Constant-time password check. |
| `motor` | `components/motor` | Forcedoor UART protocol (`motor_proto.c`, pure codec) and the UART driver task (`motor.c`). |
| `ble_gdp` | `components/ble_gdp` | NimBLE GATT server and advertising. It only handles transport. |
| `net_wifi` | `components/net_wifi` | Wi-Fi station with exponential back-off reconnect. |
| `ha_mqtt` | `components/ha_mqtt` | MQTT client, Home Assistant discovery, LWT availability, commands. |
| app | `main/` | `app_door` (actions and state fan-out), `app_ble_cmd` (BLE command protocol), `app_net` (applies settings), `app_ota` (firmware update over BLE), `app_supervisor` (watchdog, idle disconnect, periodic reboot). |

The components don't depend on each other. Only `main/` wires them together, and pins and timings
are set in `main/Kconfig.projbuild` (`idf.py menuconfig` → *Garage Door Controller*).

## Hardware

Wiring follows the garage-esp32 (ESPHome) reference. The Forcedoor motor's USB-A socket is **not USB**:

| Motor USB pin | Signal | Connect to |
|---|---|---|
| VBUS | **24 V** | 24 V→5 V buck → board 5V |
| D- | motor TX (5 V TTL) | 5k/10k divider → `GDC_MOTOR_RX_GPIO` (default **GPIO4**) |
| D+ | motor RX | `GDC_MOTOR_TX_GPIO` (default **GPIO5**) |
| GND | GND | GND |

```
motor D- ---[5k]---+--- GPIO4
                   |
                 [10k]
                   |
                  GND
```

Measure 24 V before connecting anything. Avoid C3 strapping pins 2/8/9 and USB pins 18/19.
UART: 115200 8N1.

Wi-Fi: if the AP stops answering during authentication (3 failures in a row), the controller
switches to 8.5 dBm TX power and back. Many ESP32-C3 SuperMini boards have a poorly matched
antenna and cannot join at full power. The log shows `retrying at reduced TX power`, and
`Connected ... (reduced TX power)` when that was what fixed it.

### Status LED

Plain LED on GPIO8 (menuconfig *Garage Door Controller → Status LED*, active-low by default):

| LED | Meaning |
|---|---|
| Off | No Wi-Fi configured |
| Blinking (2 Hz) | Searching for / reconnecting to Wi-Fi |
| Double blink (every 1.2 s) | Wi-Fi connected, MQTT broker configured but not connected |
| Solid | Connected to Wi-Fi (and to the broker, if one is configured) |
| Short flash (inverted) | A command was sent to the motor (open, close, stop, light) |

### Motor protocol

`[hdr][len][payload][sum][0x0D]`, where `len` is the whole frame length and `sum` is the sum of
all preceding bytes & 0xFF.

| Command | Frame |
|---|---|
| Open | `23 07 41 00 00 6B 0D` |
| Stop | `23 07 41 01 00 6C 0D` |
| Close | `23 07 41 02 00 6D 0D` |
| LED on / off | `23 07 41 F0 00 5B 0D` / `23 07 41 F1 00 5C 0D` |

Status frames are 17 bytes long: `40 11 49 <op> <led> … <pos%> … <sum> 0D`. `op` is 0 opening,
1 stopped, 2 closing. `led` is F0 on, F1 off. The motor echoes each accepted command as a
`40 07 41 <cmd> …` frame.

## Build / flash

ESP-IDF v6.1, target `esp32c3`:

```bash
idf.py set-target esp32c3
```
```bash
idf.py build flash monitor
```

`espressif/mqtt` is pulled from the component registry on the first build.

The version is set in `main/version.h`. CMake copies it into the image (`esp_app_desc_t`), where
the phone app reads it from its bundled copy.

### Firmware updates over BLE

Flash layout (`partitions.csv`): two 1.875 MB OTA slots, plus `nvs` at the same offset and size as
the old single-app table.

**Migrating a controller from 1.0.0:** it has no OTA slots, so flash it over USB once with
`idf.py flash`. Settings and bonded phones are kept because `nvs` did not move. After that, every
update can go over BLE.

**Releasing an update through the app:**
1. Bump `VERSION` in `main/version.h` and run `idf.py build`.
2. In GarageDoorBLE-App, run `gradlew copyFirmware`. This copies `build/GarageDoorBLE.bin` to
   `app/src/main/assets/firmware/`.
3. Build and ship the app. Settings → Firmware offers the update when the bundled version is newer
   than the controller's.

**Rollback:** the bootloader starts a new image in trial mode. It keeps the image after the first
successful BLE AUTH, or after `GDC_OTA_CONFIRM_AFTER_S` (default 10 min) of uptime. If the image
crashes or resets before either of those, the bootloader switches back to the previous firmware.
`esp_ota_end` checks the SHA-256 appended to the image and the chip. The image must also have the
same project name (`GarageDoorBLE`) as the running firmware.

## Phone app: BLE protocol

The device advertises the GDP service UUID with its name (default `BLEDoor`) in the scan response.
Pairing (bonding, LE Secure Connections, Just Works) is started by the phone.

| Characteristic | Props | Content |
|---|---|---|
| Service `713D0000-503E-4C75-BA94-3148F18D941E` | | |
| Command `713D0003-…` | write, write-no-rsp, read | write `[cmd][payload]`, then read `[status][data]` |
| State `713D0002-…` | read, notify | 4 bytes: position % (FF unknown), op (0 opening / 1 stopped / 2 closing / FF), LED (1/0/FF), flags (bit0 Wi-Fi configured, bit1 Wi-Fi up, bit2 MQTT up) |
| Data `713D0004-…` | write, write-no-rsp | Firmware image chunks: `<offset u32 LE><bytes>`, only during an OTA |

Every command except AUTH needs a successful AUTH on the current connection.

| Cmd | Payload | Action |
|---|---|---|
| `0x01` AUTH | password | Unlocks the other commands until disconnect |
| `0x02` DOOR | 1 byte action | 0 open, 1 stop, 2 close, 4 LED on, 5 LED off, 6 toggle (stop if moving, else open/close) |
| `0x03` SET | key, flags, value bytes | flags bit0 = first chunk, bit1 = last chunk; `0x03` for a single write. Chunk values longer than MTU−6. |
| `0x04` GET | key | data: value. Secrets come back as `********` when set. |
| `0x05` APPLY | – | Reconnect Wi-Fi and MQTT using the saved settings |
| `0x06` INFO | – | data: `fw=..;name=..;wifi=connected;ssid=..;ip=..;rssi=..;mqtt=1;pos=..;op=..;led=..` |
| `0x07` REBOOT | – | Restart the controller |
| `0x08` FACTORY RESET | – | Erase settings and bonded phones, reboot |
| `0x09` OTA_BEGIN | size u32 LE | Open the inactive OTA slot. `0x02` if the image does not fit |
| `0x0A` OTA_STATUS | – | data: next expected offset (u32 LE). `0x03` once a flash write failed |
| `0x0B` OTA_END | – | Verify the image, select it for boot, then reboot |
| `0x0C` OTA_ABORT | – | Drop the update. Disconnecting does the same |

Reply status byte: `0x00` OK, `0x01` auth (wrong password / not authenticated), `0x02` invalid
payload, `0x03` failed, `0x04` unknown command. **A wrong password drops the link after 1 s.**

Keys: 1 Wi-Fi SSID, 2 Wi-Fi password (empty or 8–64), 3 MQTT URI (`mqtt://192.168.1.10:1883`,
`mqtts://…`), 4 MQTT user, 5 MQTT password, 6 device name (≤20), 7 door password (≤20).

OTA transfer: after OTA_BEGIN, the phone sends a window of chunks (32 × MTU−7 bytes) as
write-without-response on the data characteristic, then asks OTA_STATUS. The controller drops any
chunk whose offset is not the next expected byte, so the phone resends from the offset that
OTA_STATUS returns. When the offset reaches the image size, the phone sends OTA_END.

Example provisioning sequence:
```
01 <pw>                        AUTH
03 01 03 "MyWiFi"              SET SSID
03 02 03 "wifipassword"        SET Wi-Fi password
03 03 03 "mqtt://10.0.0.5:1883"
03 04 03 "garage"              MQTT user
03 05 03 "secret"              MQTT password
05                             APPLY
06                             INFO -> check wifi=connected;mqtt=1
```

## Home Assistant

1. Install the Mosquitto broker add-on and the MQTT integration. Create an HA user for the device.
2. Provision the URI, user and password over BLE, then send APPLY.
3. The device appears under **MQTT → Devices** with the device name.

Entities: **Cover** (garage: open/close/stop with position), **LED** light,
**Restart** button, **Position** sensor, **Operation** sensor, **Wi-Fi signal** sensor.
Availability uses an LWT on `garage_door/<node_id>/status`.

The node id is `gdc_` plus the last 3 bytes of the MAC.

Topics are `garage_door/<node_id>/door/set|state|position|operation`, `led/set|state`,
`restart/press` and `rssi`.
