# Garage Door Controller V3

ESP32-C3 firmware that replaces the Forcedoor Wi-Fi dongle. It talks to the motor unit over its
UART port and can be controlled two ways:

- **Bluetooth (NimBLE):** the phone app. Works with no network at all. It is also how you set up
  Wi-Fi, MQTT, the device name and the door password.
- **Home Assistant over Wi-Fi:** MQTT with auto-discovery.

Password-based control from V1 still works: the existing app commands and GATT UUIDs are unchanged.

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
| app | `main/` | `app_door` (actions and state fan-out), `app_ble_cmd` (BLE command protocol), `app_net` (applies settings), `app_supervisor` (watchdog, idle disconnect, periodic reboot). |

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

### Motor protocol

`[hdr][len][payload][sum][0x0D]`, where `len` is the whole frame length and `sum` is the sum of
all preceding bytes & 0xFF.

| Command | Frame |
|---|---|
| Open | `23 07 41 00 00 6B 0D` |
| Stop | `23 07 41 01 00 6C 0D` |
| Close | `23 07 41 02 00 6D 0D` |
| Partial open | `23 07 41 03 00 6E 0D` |
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

## Phone app: BLE protocol

The device advertises as the configured name (default `BLEDoor`). The advertisement carries the
GDP 128-bit service UUID and the "AWI" service data, the same as V1. Pairing is bonding with Just
Works, as in V1.

| Characteristic (UUID, LSB-first bytes as in V1) | Props | Content |
|---|---|---|
| Service `…50 00 00 3D 71` | | |
| Data `…50 03 00 3D 71` | write, write-no-rsp, read | write `[cmd][payload]`, then read the reply |
| State `…50 02 00 3D 71` | read, notify | 1 byte: bit0 = door closed, bit1 = input mode (V1) |
| Ext state `…50 04 00 3D 71` | read, notify | 4 bytes: position % (FF unknown), op (0 opening/1 stopped/2 closing/FF), LED (1/0/FF), flags (bit0 Wi-Fi configured, bit1 Wi-Fi up, bit2 MQTT up) |

The reply is `"OK\r\n\0"` or `"ERROR\r\n\0"`, byte-identical to V1, or data for GET/INFO. **Any
failed command replies ERROR and the device drops the link after 1 s.**

### V1 commands (unchanged)

| Cmd | Payload | Action |
|---|---|---|
| `0x00` or `'0'` | password | Toggle door. Moving → stop, closed → open, open → close. |
| `0x01` | password, then new name within 2 s | Set device (BLE) name |
| `0x02` | password, then new password within 1 s | Set door password |
| `0x03` | – | Reboot |
| `0x04` | `'0'`/`'1'` | Input mode. Stored and reported in state bit1 only, because there is no GPIO input any more. |

### V3 commands (send AUTH once per connection)

| Cmd | Payload | Action |
|---|---|---|
| `0x10` AUTH | password | Unlocks the commands below until disconnect |
| `0x11` DOOR | 1 byte action | 0 open, 1 stop, 2 close, 3 partial, 4 LED on, 5 LED off, 6 toggle |
| `0x12` SET | key, flags, value bytes | flags bit0 = first chunk, bit1 = last chunk. Use `0x03` for a single write. Chunk values that are longer than MTU−6. |
| `0x13` GET | key | Reply: value + NUL. Secrets come back as `********`. |
| `0x14` APPLY | – | Reconnect Wi-Fi and MQTT using the saved settings |
| `0x15` INFO | – | Reply: `fw=..;name=..;wifi=connected;ssid=..;ip=..;rssi=..;mqtt=1;pos=..;op=..;led=..` |
| `0x16` FACTORY RESET | – | Erase all settings and reboot |

Keys: 1 Wi-Fi SSID, 2 Wi-Fi password (empty or 8–64), 3 MQTT URI (`mqtt://192.168.1.10:1883`,
`mqtts://…`), 4 MQTT user, 5 MQTT password, 6 device name (≤20), 7 door password (≤20).

Example provisioning sequence:
```
10 <pw>                        AUTH
12 01 03 "MyWiFi"              SET SSID
12 02 03 "wifipassword"        SET Wi-Fi password
12 03 03 "mqtt://10.0.0.5:1883"
12 04 03 "garage"              MQTT user
12 05 03 "secret"              MQTT password
14                             APPLY
15                             INFO -> check wifi=connected;mqtt=1
```

## Home Assistant

1. Install the Mosquitto broker add-on and the MQTT integration. Create an HA user for the device.
2. Provision the URI, user and password over BLE, then send APPLY.
3. The device appears under **MQTT → Devices** with the device name.

Entities: **Cover** (garage: open/close/stop with position), **LED** light, **Partial open** button,
**Restart** button, **Position** sensor, **Operation** sensor, **Wi-Fi signal** sensor.
Availability uses an LWT on `garage_door/<node_id>/status`.

The node id is `gdc_` plus the last 3 bytes of the MAC.

Topics are `garage_door/<node_id>/door/set|state|position|operation`, `led/set|state`,
`partial/press`, `restart/press` and `rssi`.

## Behaviour changes from V1

- Bluedroid is replaced by NimBLE. The UUIDs, advertising data, replies and command bytes are
  unchanged.
- The GPIO relay and reed input are replaced by the motor's UART protocol. "Open" is now a smart
  toggle, and the real position is reported.
- A phone that is idle for 5 min is disconnected. V1 rebooted the whole device in that case.
- The 24 h periodic reboot is kept, but it only runs when no phone is connected and the door isn't
  moving. Set it to 0 in menuconfig to disable it.
- The V1 anti-tamper `verify_config` obfuscation was removed.
- NVS writes are committed properly, and the name length is checked against the correct limit. V1
  could overflow here.
