# Wiring and hardware

## Safety boundary

The RP2040 and Pixhawk signals are control logic only. They must remain
galvanically isolated from the traction/high-voltage circuit. A normally-closed
mechanical E-stop should directly interrupt the EV200AAANA coil circuit so that
the contactor opens without software, radio, RP2040, or Pixhawk cooperation.

The E-stop in the operating diagram has two separate contact paths:

- Its **NC contact** is in series with the EV200 contactor coil. Pressing the
  E-stop opens this path and removes contactor-coil power.
- Its **NO contact** switches the auxiliary `5V+` sensing supply into the
  low-level sensing relay. That relay provides the isolated 3.3 V kill signal
  to the Pixhawk A3 input.

The `5V+` annotation therefore describes the E-stop sensing circuit, not the
EV200 coil voltage. The 5 V sensing node must not be connected directly to a
3.3 V Pixhawk input.

Use a branch fuse sized for the coil wiring and switching device. Verify relay
contact ratings against coil inrush, DC voltage, temperature, and expected
cycle life. Do not energize the traction circuit during initial tests.

## Operator/transmitter RP2040-Zero

### SiK radio UART

| RP2040 | SiK radio |
|---|---|
| GP0 TX | RX |
| GP1 RX | TX |
| GND | GND |
| Appropriate regulated supply | VCC, per the radio model |

UART settings: 57,600 baud, 8 data bits, no parity, 1 stop bit.

### Normally-open buttons

Each button bridges a HIGH output pin to an input configured with an internal
pull-down.

| Function | Button terminal 1 | Button terminal 2 |
|---|---:|---:|
| RIGHT / KILL | GP4 | GP5 |
| LEFT / RESET | GP6 | GP7 |

Released inputs read LOW; pressed inputs read HIGH.

### LCDwiki 1.9-inch ST7789 display

| LCD pin | RP2040-Zero | Function |
|---|---:|---|
| GND | GND | Ground |
| VCC | 3V3 | Display power |
| SCL | GP10 | SPI1 clock |
| SDA | GP11 | SPI1 MOSI |
| RES | GP12 | Reset |
| DC | GP13 | Data/command |
| CS | GP14 | Chip select |
| BLK | 3V3 | Backlight always on |

This module is a 3.3 V device. Do not connect VCC or BLK to 5 V.

## Vehicle/receiver RP2040

### SiK radio UART

| RP2040 | SiK radio |
|---|---|
| GP0 TX | RX |
| GP1 RX | TX |
| GND | GND |

### State outputs

| RP2040 | Destination | RESET/START | KILL/STOP |
|---|---|---:|---:|
| GP10 | Pixhawk 6X AUX5 through 1 kΩ series resistor | HIGH | LOW |
| GP13 | Isolated relay/driver logic input | HIGH | LOW |

Install an external pull-down on each safety-relevant output so power-up,
disconnection, and an unpowered RP2040 resolve to LOW/KILL. Confirm the exact
pull-down value against the input module; 10 kΩ is a typical logic-level
starting point, not a universal value.

The RP2040 GPIO pins are 3.3 V logic and are not 5 V tolerant. GP13 must feed a
compatible isolated input or transistor/MOSFET driver. It must never power a
relay or contactor coil directly.

For Pixhawk 6X AUX5, disable the servo function before wiring the signal:

```text
SERVO13_FUNCTION = -1
BTN_ENABLE = 1
BTN_PIN1 = 54
```

Reboot ArduPilot after changing the servo function. Validate AUX5 voltage and
logic with a multimeter before assigning any action. If an ArduPilot button
function expects active-high KILL, invert it because this circuit uses
LOW = KILL.

## EV200AAANA contactor

The TE Connectivity EV200AAANA is a normally-open SPST high-voltage DC
contactor with a **9–36 VDC coil**, internal economizer, and polarized flying
leads. The manufacturer lists up to 3.8 A coil inrush and approximately 0.13 A
holding current at 12 V (0.07 A at 24 V). Size the coil supply, fuse, E-stop,
connector, wiring, and relay/driver for the inrush and DC switching duty.

In the supplied diagram, the contactor coil is fed from the protected
14.8/22.2 V control supply through the telemetry relay and the E-stop's NC
contact. This falls within the nominal 9–36 V coil range, subject to wiring
losses and the complete TE operating specification. The nearby `5V+` node is
only for the separate E-stop NO sensing circuit. Observe the manufacturer's
lead polarity and instructions. Do not assume the headline 500 A/900 V values
apply to every load; breaking capability depends on voltage, current, time
constant, temperature, conductor layout, and protection system.

Product reference: [TE EV200AAANA, part 1618002-7](https://www.te.com/en/product-1618002-7.html).

## Vehicle-level A1/A2 indication logic

The operating diagram defines the following Pixhawk output truth table. This
logic is external to the RP2040 sketches and must be configured and verified in
ArduPilot separately.

| A1 | A2 | Indicated state |
|---:|---:|---|
| 0 | 0 | KILL |
| 1 | 0 | MANUAL |
| 1 | 1 | AUTO |
| 0 | 1 | KILL |
