# SiK radio configuration

Configure both 915 MHz radios with matching link parameters. The frequency
limits below are intentionally not prescribed: retain the values selected for
the applicable local regulations and hardware band.

| Setting | Value |
|---|---:|
| Format | Leave firmware default/current value |
| Baud / SERIAL_SPEED | 57,600 (`57`) |
| Air Speed | `64` |
| Net ID | `50` |
| TX Power | Legal value for the installation |
| ECC | Off (`0`) |
| MAVLink | MAVLink / `1` |
| Opportunistic resend | Off (`0`) |
| Min/Max Frequency | Leave regulatory values unchanged |
| Number of Channels | `50`, if valid for the retained band plan |
| Duty Cycle | `100`, only where permitted |
| LBT RSSI | Regulatory value; currently `0` |
| RTS/CTS | Off (`0`) |
| Max Window | `131 ms` |
| AES | Off unless deliberately provisioned on both radios |

The following must match between the two radios: firmware compatibility, air
speed, frequency limits, channel count, Net ID, ECC, LBT RSSI, and maximum
window. Save settings to both radios, reboot them, then load settings again to
verify persistence.

`MAVLINK=1` is required for automatic `RADIO_STATUS` injection. Each RP2040
sends a valid MAVLink heartbeat once per second to activate reporting. RSSI
display is diagnostic only and is not used by the KILL decision.

## RSSI labels

- **RSSI L**: signal received by the SiK radio attached to the operator unit.
- **RSSI R**: signal received by the remote/vehicle SiK radio.

SiK RSSI values use a device-specific 0–254 scale; higher is stronger. A value
of 255 indicates unavailable/unknown. Link safety is determined by valid
application heartbeat/ACK traffic, not an RSSI threshold.

Reference: [ArduPilot SiK advanced configuration](https://ardupilot.org/copter/docs/common-3dr-radio-advanced-configuration-and-technical-information.html).
