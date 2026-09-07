# F334 Supercapacitor CAN Protocol

This document describes the wire contract between the NSEonLu F407 chassis controller and the independent F334 supercapacitor controller.

## Bus configuration

- Classic CAN, standard 11-bit identifiers.
- F407 command ID: `0x061`.
- F334 status ID: `0x051`.
- Both frames use DLC 8.
- Multi-byte values use little-endian byte order.
- F407 sends commands from `ChassisTask()` every 5 ms (200 Hz).
- F334 initially sends status every 1 ms (1 kHz); after bus validation it sends every 5 ms (200 Hz).
- F407 declares the F334 status offline after 200 ms without a valid frame.

## Command frame: F407 to F334, ID 0x061

| Byte | Format | Meaning |
|---:|---|---|
| 0 | bit field | bit0: enable DCDC; bit1: restart request; bit2-7: zero |
| 1-2 | `uint16_t`, little-endian | original referee chassis power limit in W |
| 3-4 | `uint16_t`, little-endian | original referee buffer energy in J |
| 5-7 | reserved | zero |

The normal runtime encoder always leaves the restart-request bit clear. Restart is not part of periodic chassis control.

Example: enable DCDC, `100 W` power limit, `50 J` buffer energy:

```text
01 64 00 32 00 00 00 00
```

The command must contain the original referee limit, or the explicit 40 W bench fallback when bench mode is enabled and referee data is absent. It must never contain the supercapacitor-boosted motor budget.

## Status frame: F334 to F407, ID 0x051

| Byte | Format | Meaning |
|---:|---|---|
| 0 | bit field | bit7: DCDC output disabled; bit0-6: error code |
| 1-4 | IEEE-754 `float`, little-endian | measured chassis power in W |
| 5-6 | `uint16_t`, little-endian | F334-reported available chassis power in W |
| 7 | `uint8_t` | capacitor energy, `0-255` maps to `0-100%` |

Status error bits:

| Bit | Mask | Meaning |
|---:|---:|---|
| 0 | `0x01` | under-voltage |
| 1 | `0x02` | over-voltage |
| 2 | `0x04` | buck-boost fault |
| 3 | `0x08` | short circuit |
| 4 | `0x10` | high temperature |
| 5 | `0x20` | no power input |
| 6 | `0x40` | capacitor fault |
| 7 | `0x80` | DCDC output disabled |

The receiver reconstructs the float through an aligned `uint32_t` plus `memcpy`; it does not cast the CAN buffer to a packed structure. Frames with DLC other than 8 or a non-finite chassis-power float are rejected and do not refresh the online watchdog.

## Power-budget behavior

- Energy at or below 10%: no boost.
- Energy from 10% to 30%: boost scales linearly from 0% to 100%.
- Energy at or above 30%: full permitted boost.
- F334-reported boost above the referee limit is capped at 100 W.
- Budget decreases apply immediately; increases are limited to 200 W/s.
- The existing motor-model safety factor remains 0.95.

F334 offline, disabled, low-energy, or faulted: remove supercapacitor boost immediately and retain only the valid referee/bench base budget.

The capacitor state does not call `DJIMotorStop()` directly. Chassis stop behavior remains owned by robot emergency-stop, referee output permission, and motor-offline logic.
