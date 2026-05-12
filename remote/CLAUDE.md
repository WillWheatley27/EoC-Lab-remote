# CLAUDE.md — EoC Lab BLE Remote (ESP32-C3)

Companion remote for the main board recorder in the parent directory.
Two buttons + OLED + BLE advertiser + deep sleep. Pure ESP-IDF, no
Arduino, no external components.

## Hardware

Target: **ESP32-C3** (any variant — the project uses only common
peripherals).

### Pin map (defined in `main/remote_pins.h`)

| Function                | GPIO   | Notes                              |
| ----------------------- | ------ | ---------------------------------- |
| ON/OFF button           | 4      | Active low, internal pullup        |
| PAUSE button            | 5      | Active low, internal pullup        |
| OLED SDA (SSD1306)      | 6      | I2C0, 100 kHz, addr 0x3C, 128x32   |
| OLED SCL (SSD1306)      | 7      | I2C0                               |

Reserved (do **not** assign here): GPIO 2, 8, 9 (ESP32-C3 strapping
pins). `remote_pins.h` has a compile-time `#error` guard against
misuse.

Both buttons share an internal-pullup-to-VCC, switch-to-GND wiring.
EXT1 deep-sleep wake is configured for both pins as
`ESP_EXT1_WAKEUP_ALL_LOW`.

## Behaviour

State diagram (soft state — the remote has no feedback channel from
the main board, so it just trusts that the main obeys):

```
IDLE  --ON/OFF-->  RECORDING  --PAUSE-->  PAUSED
IDLE  <--ON/OFF--  RECORDING  <--PAUSE--  PAUSED
                   RECORDING  <--ON/OFF-- PAUSED   (also resets to IDLE)
PAUSE while IDLE = no state change (main ignores TT=01 in idle)
```

OLED contents:
- `IDLE`      → `AWAKE / READY`
- `RECORDING` → `RECORDING`
- `PAUSED`    → `PAUSED`
- Right before deep sleep → cleared

Sleep:
- 30 s of no button presses → clear OLED, deinit I2C, deep sleep.
- Either button wakes the C3 via EXT1.
- After wake, the originating button press is synthesized as the
  first queue event so the trigger goes out without a second press.
- After advertising, the 30 s inactivity timer restarts.

Advertising:
- 500 ms window per button press.
- Non-connectable, general-discoverable.
- Single 128-bit service UUID payload, format below.

## BLE timestamp UUID format

```
00000000-0000-0000-TT00-YYMMDDHHMMSS
```

Big-endian byte layout:
- `[0..7]   = 0x00`            — fixed prefix the main parser filters on
- `[8]      = TT`              — `0x01` pause, `0x02` on/off
- `[9]      = 0x00`
- `[10..15] = YY MM DD HH MM SS` (BCD, 2000+YY)

Built in `remote_ble.c::s_build_trigger_uuid` from
`remote_rtc_now_bcd6()`. The main board parser
(`main/ble_trigger.c::s_ble_extract_trigger`) is the source of truth
for this format.

## RTC

The C3 RTC keeps running on the slow internal RC oscillator across
deep sleep, but is reset on power-cycle / re-flash.

- On `ESP_RST_POWERON` only, `remote_rtc_init()` seeds the RTC from
  `__DATE__` / `__TIME__` (firmware build epoch). On any other reset
  reason — including wake-from-deep-sleep — the RTC is left alone.
- For runtime correction without re-flashing, call
  `remote_rtc_set_from_string("YYMMDDHHMMSS")` from a UART command or
  similar. (Not wired into the menu yet — extend `remote_main.c` if
  you need it interactively.)
- Drift: a few seconds per hour on the internal RC. Acceptable for
  filename / event-log resolution; not for forensics.

## File layout

```
remote/
├── CMakeLists.txt              # ESP-IDF project root
├── sdkconfig.defaults          # ESP32-C3 + NimBLE broadcaster role
├── CLAUDE.md                   # this file
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    ├── remote_pins.h           # all GPIO defines, compile-time guard
    ├── remote_main.c           # state machine + sleep
    ├── remote_oled.{c,h}       # SSD1306 driver (own I2C bus)
    ├── remote_buttons.{c,h}    # 50 ms debounced poll task + queue
    ├── remote_rtc.{c,h}        # build-epoch seed + BCD encoder
    └── remote_ble.{c,h}        # NimBLE NONCONN_IND advertiser
```

## Build / flash

```bash
cd remote
idf.py set-target esp32c3
idf.py build
idf.py -p <PORT> flash monitor
```

The build does **not** compile if any of GPIO 2/8/9 is assigned to a
remote function in `remote_pins.h` — see the `#error` guard at the
bottom of that file.

## Pairing with the main board

There is no actual pairing — the channel is one-way BLE advertising.
The main board's NimBLE scanner in
`../main/ble_trigger.c` filters every advertisement against the 8-byte
zero prefix and 0x01/0x02 TT byte; everything else is ignored. Any
number of remotes can co-exist; the last one to advertise wins (with
500 ms BLE-side debounce on the main board).

## Power

Sleep current expectation: **<50 µA** when the OLED is off and the I2C
driver is deinitialized. Higher numbers usually mean the OLED display-
on bit (0xAF) was left set or the I2C peripheral was left running —
check `remote_oled_deinit()` and the `s_enter_deep_sleep()` ordering
in `remote_main.c`.

Active-while-advertising current is dominated by the BLE radio (~10
mA peaks during adv events). At 30 ms adv interval × 500 ms window
that is roughly 16 adv events per press — plenty for the main board
scanner with its 80/8 unit interval/window to receive several.
