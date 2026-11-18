# Radio protocol and state machine

The application protocol is line-oriented ASCII at 57,600 baud. MAVLink frames
inserted by the SiK radios share the UART; the firmware separates binary
MAVLink packets from these text messages.

## Messages

```text
CMD,<sequence>,KILL
CMD,<sequence>,RESET
HB,<sequence>,KILL
HB,<sequence>,RESET
ACK,<sequence>,KILL
ACK,<sequence>,RESET
```

- `CMD` is sent immediately after a debounced button press.
- `HB` is sent every 250 ms and carries the transmitter's latched request.
- `ACK` reports the receiver's actual state, not merely the requested state.
- The sequence number is informational in this version; freshness is based on
  the arrival of a completely valid packet.

## Receiver transitions

| Current condition | Input | Result |
|---|---|---|
| Any state | `CMD ... KILL` or `HB ... KILL` | KILL immediately |
| KILL | `HB ... RESET` | Remain KILL |
| KILL | Fresh `CMD ... RESET` | RESET/START |
| RESET | Valid RESET heartbeat | Remain RESET |
| Any connected state | No valid packet for 2 s | KILL and mark link lost |
| KILL after link loss | Link recovers | Remain KILL until fresh RESET command |
| Power-up/watchdog reset | — | KILL |

The receiver's GP10 and GP13 outputs are both LOW in KILL and HIGH in RESET.

## MAVLink coexistence

Both nodes emit a MAVLink v1 heartbeat once per second solely to enable SiK
radio-status injection. The transmitter accepts both message 109
(`RADIO_STATUS`) and the older ArduPilotMega message 166 (`RADIO`), reading
payload offsets 4 and 5 for local and remote RSSI. Incoming MAVLink data is not
allowed to refresh the application safety timer.
