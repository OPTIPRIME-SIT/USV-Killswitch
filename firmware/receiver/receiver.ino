#include <Arduino.h>

constexpr uint8_t RADIO_TX = 0;
constexpr uint8_t RADIO_RX = 1;
constexpr uint32_t RADIO_BAUD = 57600;

// Direct 3.3 V logic connection to Pixhawk AUX5 through a series resistor.
// With the external pull-down: HIGH = RESET/START, LOW = KILL/STOP.
constexpr uint8_t PIXHAWK_STATE_PIN = 10;

// Additional RESET/KILL output.
// HIGH = RESET, LOW = KILL.
constexpr uint8_t KILL_STATE_PIN = 13;

constexpr uint32_t LINK_TIMEOUT_MS = 2000;
constexpr uint32_t STATUS_INTERVAL_MS = 1000;
constexpr uint32_t MAVLINK_HEARTBEAT_INTERVAL_MS = 1000;

char radioLine[64];
size_t radioLineLength = 0;
bool relayRunning = false;
bool linkConnected = false;
uint32_t lastValidContact = 0;
uint32_t lastStatusReport = 0;
uint32_t lastMavlinkHeartbeat = 0;

// SiK with S6 MAVLINK=1 can insert binary MAVLink packets into the UART
// stream. Discard complete packets so they cannot contaminate CMD/HB lines.
bool mavlinkPacket = false;
uint8_t mavlinkMagic = 0;
uint8_t mavlinkIndex = 0;
uint16_t mavlinkBytesRemaining = 0;

uint16_t mavlinkChecksumAccumulate(uint8_t byte, uint16_t checksum) {
  uint8_t temp = byte ^ static_cast<uint8_t>(checksum & 0xFF);
  temp ^= temp << 4;
  return (checksum >> 8) ^ (static_cast<uint16_t>(temp) << 8) ^
         (static_cast<uint16_t>(temp) << 3) ^ (temp >> 4);
}

void sendMavlinkHeartbeat() {
  static uint8_t mavlinkSequence = 0;
  const uint8_t frame[] = {0xFE, 9, mavlinkSequence++, 202, 191, 0,
                           0, 0, 0, 0, 18, 0, 0, 4, 3};
  uint16_t checksum = 0xFFFF;
  for (uint8_t i = 1; i < sizeof(frame); ++i) checksum = mavlinkChecksumAccumulate(frame[i], checksum);
  checksum = mavlinkChecksumAccumulate(50, checksum);
  Serial1.write(frame, sizeof(frame));
  Serial1.write(static_cast<uint8_t>(checksum & 0xFF));
  Serial1.write(static_cast<uint8_t>(checksum >> 8));
}

void discardMavlinkByte(uint8_t byte) {
  if (!mavlinkPacket) {
    if (byte != 0xFE && byte != 0xFD) return;
    mavlinkPacket = true;
    mavlinkMagic = byte;
    mavlinkIndex = 0;
    return;
  }
  ++mavlinkIndex;
  if (mavlinkIndex == 1) {
    mavlinkBytesRemaining = byte + (mavlinkMagic == 0xFE ? 6 : 11);
    return;
  }
  if (--mavlinkBytesRemaining == 0) mavlinkPacket = false;
}

void reportRelayState() {
  Serial.println(relayRunning ? "RELAY STARTED" : "RELAY STOPPED");
}

void setSystemState(bool resetState) {
  relayRunning = resetState;
  digitalWrite(PIXHAWK_STATE_PIN, resetState ? HIGH : LOW);
  digitalWrite(KILL_STATE_PIN, resetState ? HIGH : LOW);
  reportRelayState();
}

void forceKill(const char* reason) {
  setSystemState(false);
  Serial.print("KILL REASON: ");
  Serial.println(reason);
}

void reportStatus() {
  Serial.println(relayRunning ? "STATUS RELAY STARTED" : "STATUS RELAY STOPPED");
  Serial.print("LINK=");
  Serial.println(linkConnected ? "CONNECTED" : "LOST");
  Serial.print("GPIO10=");
  Serial.println(digitalRead(PIXHAWK_STATE_PIN) == HIGH ? "HIGH" : "LOW");
  Serial.print("GPIO13=");
  Serial.print(digitalRead(KILL_STATE_PIN) == HIGH ? "HIGH" : "LOW");
  Serial.println(relayRunning ? " RESET" : " KILL");
  lastStatusReport = millis();
}

void sendAcknowledgement(uint32_t sequence) {
  Serial1.print("ACK,");
  Serial1.print(sequence);
  Serial1.print(',');
  Serial1.println(relayRunning ? "RESET" : "KILL");
}

void processRadioLine(char* line) {
  char packetType[4];
  unsigned long sequence;
  char requestedState[8];

  if (sscanf(line, "%3[^,],%lu,%7s", packetType, &sequence, requestedState) != 3) {
    Serial.println("RADIO PARSE ERROR");
    return;
  }

  if (strcmp(packetType, "CMD") != 0 && strcmp(packetType, "HB") != 0) return;
  if (strcmp(requestedState, "RESET") != 0 &&
      strcmp(requestedState, "KILL") != 0) return;

  // Only a completely valid packet refreshes the failsafe timer.
  lastValidContact = millis();
  if (!linkConnected) {
    linkConnected = true;
    Serial.println("OPERATOR LINK CONNECTED");
  }

  const bool isCommand = strcmp(packetType, "CMD") == 0;
  const bool requestedReset = strcmp(requestedState, "RESET") == 0;

  // KILL may be commanded by either a command or heartbeat. RESET is
  // interlocked: only a fresh, explicit command can clear the KILL state.
  if (!requestedReset) {
    if (relayRunning) setSystemState(false);
  } else if (isCommand && !relayRunning) {
    setSystemState(true);
  }

  if (isCommand) {
    Serial.print("COMMAND RECEIVED: ");
    Serial.println(requestedState);
  }

  // Report the receiver's actual state, which may remain KILL after a
  // RESET heartbeat following link recovery.
  sendAcknowledgement(sequence);
}

void readRadio() {
  while (Serial1.available()) {
    const uint8_t byte = static_cast<uint8_t>(Serial1.read());
    if (mavlinkPacket || byte == 0xFE || byte == 0xFD) {
      discardMavlinkByte(byte);
      continue;
    }
    const char c = static_cast<char>(byte);
    if (c != '\r' && c != '\n' && (byte < 0x20 || byte > 0x7E)) continue;
    if (c == '\r') continue;

    if (c == '\n') {
      radioLine[radioLineLength] = '\0';
      processRadioLine(radioLine);
      radioLineLength = 0;
    } else if (radioLineLength < sizeof(radioLine) - 1) {
      radioLine[radioLineLength++] = c;
    } else {
      radioLineLength = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Preload LOW before enabling the output so the pull-down/KILL state is
  // retained during boot or a receiver watchdog reset.
  digitalWrite(PIXHAWK_STATE_PIN, LOW);
  pinMode(PIXHAWK_STATE_PIN, OUTPUT);
  digitalWrite(KILL_STATE_PIN, LOW);
  pinMode(KILL_STATE_PIN, OUTPUT);

  Serial1.setTX(RADIO_TX);
  Serial1.setRX(RADIO_RX);
  Serial1.begin(RADIO_BAUD);

  rp2040.wdt_begin(3000);
  Serial.println("Safety receiver ready");
  forceKill("STARTUP");
}

void loop() {
  rp2040.wdt_reset();
  readRadio();

  if (linkConnected && millis() - lastValidContact >= LINK_TIMEOUT_MS) {
    linkConnected = false;
    forceKill("OPERATOR LINK TIMEOUT");
  }

  if (millis() - lastStatusReport >= STATUS_INTERVAL_MS) {
    reportStatus();
  }

  if (millis() - lastMavlinkHeartbeat >= MAVLINK_HEARTBEAT_INTERVAL_MS) {
    lastMavlinkHeartbeat = millis();
    sendMavlinkHeartbeat();
  }
}
