# Commissioning and testing

Perform these tests with the traction/high-energy circuit isolated. Use LEDs or
a low-power test load in place of the EV200AAANA until all logic tests pass.

## 1. Static electrical checks

1. Confirm there is no continuity between high-voltage and logic wiring.
2. Confirm RP2040 and LCD rails are 3.3 V where required.
3. Verify SiK TX/RX are crossed and all logic grounds are intentional.
4. Verify GP10 and GP13 are held LOW by their external pull-downs while the
   receiver RP2040 is disconnected.
5. Confirm the EV200 coil path uses the protected 14.8/22.2 V control supply
   through the telemetry relay and E-stop NC contact.
6. Confirm the separate `5V+` E-stop NO sensing path terminates at the
   low-level/isolating relay and cannot apply 5 V directly to the Pixhawk.

## 2. Receiver output test

1. Power only the receiver control electronics.
2. On startup, measure GP10 and GP13: both must be LOW/KILL.
3. Do not connect GP10 to Pixhawk until `SERVO13_FUNCTION=-1` has been saved and
   the Pixhawk rebooted.

## 3. Radio and button test

1. Power both RP2040s and both SiK radios.
2. Confirm the LCD changes from `LINK: LOST` to `LINK: OK`.
3. Press LEFT once. The LCD should show `CMD: RESET` and, after acknowledgment,
   `RX: RESET`; receiver GP10/GP13 should be HIGH.
4. Press RIGHT once. Both command and receiver state should become KILL and the
   receiver outputs should be LOW.
5. Press both buttons together: KILL must take priority.

## 4. Link-loss interlock test

1. Enter RESET with a fresh LEFT press.
2. Remove power from the transmitter radio or disconnect its antenna only if
   the radio manufacturer permits operation under that condition. Prefer
   removing radio power.
3. Within approximately 2 seconds, receiver GP10/GP13 must go LOW/KILL.
4. Restore the link. The receiver must remain KILL.
5. Press LEFT again. Only this fresh RESET command may restore HIGH/RESET.

## 5. Watchdog test

Exercise each unit through power interruption and repeated resets. Every
receiver restart must begin in LOW/KILL. Confirm the external pull-down holds
KILL during the interval before firmware configures the pins.

## 6. Mechanical E-stop test

With only the protected low-voltage coil circuit energized, verify the E-stop's
normally-closed contact removes coil power regardless of RP2040, Pixhawk, radio,
or relay command. Resetting software must never bypass a latched mechanical
E-stop. Also verify that pressing the E-stop closes its separate NO sensing
contact, activates the low-level relay, and presents the intended isolated
3.3 V kill state to the Pixhawk A3 input.

## 7. Final integration

Only after all tests pass, integrate the contactor main circuit under the
supervision of a qualified electrical engineer. Add fault-injection tests for
broken wires, welded relay contacts, stuck GPIO, brownout, radio interference,
Pixhawk reboot, and loss of each power rail.
