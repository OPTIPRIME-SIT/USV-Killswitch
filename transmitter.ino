#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

constexpr uint8_t RADIO_TX = 0;
constexpr uint8_t RADIO_RX = 1;
constexpr uint32_t RADIO_BAUD = 57600;

// Normally-open buttons wired across GPIO pairs.
constexpr uint8_t RIGHT_HIGH_PIN = 4;
constexpr uint8_t RIGHT_INPUT_PIN = 5;
constexpr uint8_t LEFT_HIGH_PIN = 6;
constexpr uint8_t LEFT_INPUT_PIN = 7;

constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 250;
constexpr uint32_t LINK_TIMEOUT_MS = 2000;

// LCDwiki 1.9 inch IPS (ST7789, 170 x 320), connected to SPI1.
constexpr uint8_t LCD_SCK = 10;
constexpr uint8_t LCD_MOSI = 11;
constexpr uint8_t LCD_RESET = 12;
constexpr uint8_t LCD_DC = 13;
constexpr uint8_t LCD_CS = 14;
constexpr uint32_t DISPLAY_INTERVAL_MS = 250;
constexpr uint32_t MAVLINK_HEARTBEAT_INTERVAL_MS = 1000;

struct Button {
  uint8_t inputPin;
  bool rawState;
  bool stableState;
  uint32_t changedAt;
};

enum class LatchedState : uint8_t {
  KILL,
  RESET
};

Button rightButton{RIGHT_INPUT_PIN, HIGH, HIGH, 0};
Button leftButton{LEFT_INPUT_PIN, HIGH, HIGH, 0};

char radioLine[64];
size_t radioLineLength = 0;
uint32_t nextSequence = 1;
uint32_t lastHeartbeatSent = 0;
uint32_t lastAckReceived = 0;
bool linkEstablished = false;
bool linkWasHealthy = false;
uint32_t lastDisplayUpdate = 0;
uint32_t lastMavlinkHeartbeat = 0;
uint8_t localRssi = 0;
uint8_t remoteRssi = 0;
bool rssiAvailable = false;

Adafruit_ST7789 tft(&SPI1, LCD_CS, LCD_DC, LCD_RESET);

// MAVLink traffic inserted by SiK when S6 MAVLINK=1 is binary.  Keep it
// separate from the text CMD/HB/ACK protocol.
bool mavlinkPacket = false;
uint8_t mavlinkMagic = 0;
uint8_t mavlinkLength = 0;
uint8_t mavlinkIndex = 0;
uint16_t mavlinkBytesRemaining = 0;
uint32_t mavlinkMessageId = 0;
uint8_t mavlinkRssi = 0;
uint8_t mavlinkRemoteRssi = 0;

// Safe default after power-up or watchdog reset.
LatchedState latchedState = LatchedState::KILL;

const char* stateName() {
  return latchedState == LatchedState::RESET ? "RESET" : "KILL";
}

uint16_t mavlinkChecksumAccumulate(uint8_t byte, uint16_t checksum) {
  uint8_t temp = byte ^ static_cast<uint8_t>(checksum & 0xFF);
  temp ^= temp << 4;
  return (checksum >> 8) ^ (static_cast<uint16_t>(temp) << 8) ^
         (static_cast<uint16_t>(temp) << 3) ^ (temp >> 4);
}

void sendMavlinkHeartbeat() {
  // MAVLink 1 HEARTBEAT (message 0).  SiK uses this to enable automatic
  // RADIO_STATUS injection; the safety protocol does not depend on it.
  static uint8_t mavlinkSequence = 0;
  const uint8_t frame[] = {0xFE, 9, mavlinkSequence++, 201, 191, 0,
                           0, 0, 0, 0, 18, 0, 0, 4, 3};
  uint16_t checksum = 0xFFFF;
  for (uint8_t i = 1; i < sizeof(frame); ++i) checksum = mavlinkChecksumAccumulate(frame[i], checksum);
  checksum = mavlinkChecksumAccumulate(50, checksum);  // HEARTBEAT CRC extra.
  Serial1.write(frame, sizeof(frame));
  Serial1.write(static_cast<uint8_t>(checksum & 0xFF));
  Serial1.write(static_cast<uint8_t>(checksum >> 8));
}

void sendStatePacket(const char* packetType) {
  Serial1.print(packetType);
  Serial1.print(',');
  Serial1.print(nextSequence++);
  Serial1.print(',');
  Serial1.println(stateName());
  lastHeartbeatSent = millis();
}

void sendCommand(LatchedState newState) {
  latchedState = newState;
  Serial.print("Latched state: ");
  Serial.println(stateName());
  sendStatePacket("CMD");
}

void processRadioLine(char* line) {
  unsigned long ackSequence;
  char ackState[8];

  if (sscanf(line, "ACK,%lu,%7s", &ackSequence, ackState) != 2) return;
  if (strcmp(ackState, "RESET") != 0 && strcmp(ackState, "KILL") != 0) return;

  lastAckReceived = millis();
  if (!linkEstablished) {
    linkEstablished = true;
    Serial.println("Receiver link established");
  }
}

void processMavlinkByte(uint8_t byte) {
  if (!mavlinkPacket) {
    if (byte != 0xFE && byte != 0xFD) return;
    mavlinkPacket = true;
    mavlinkMagic = byte;
    mavlinkIndex = 0;
    mavlinkMessageId = 0;
    return;
  }

  ++mavlinkIndex;
  if (mavlinkIndex == 1) {
    mavlinkLength = byte;
    // After LEN: MAVLink 1 has 6 header/checksum bytes, MAVLink 2 has 11.
    mavlinkBytesRemaining = mavlinkLength + (mavlinkMagic == 0xFE ? 6 : 11);
    return;
  }

  if (mavlinkMagic == 0xFE && mavlinkIndex == 5) mavlinkMessageId = byte;
  if (mavlinkMagic == 0xFD && mavlinkIndex >= 7 && mavlinkIndex <= 9) {
    mavlinkMessageId |= static_cast<uint32_t>(byte) << (8 * (mavlinkIndex - 7));
  }

  const uint8_t payloadStart = mavlinkMagic == 0xFE ? 6 : 10;
  if (mavlinkMessageId == 109 && mavlinkIndex == payloadStart) mavlinkRssi = byte;
  if (mavlinkMessageId == 109 && mavlinkIndex == payloadStart + 1) mavlinkRemoteRssi = byte;

  if (--mavlinkBytesRemaining == 0) {
    if (mavlinkMessageId == 109 && mavlinkLength >= 2) {
      localRssi = mavlinkRssi;
      remoteRssi = mavlinkRemoteRssi;
      rssiAvailable = true;
    }
    mavlinkPacket = false;
  }
}

void updateDisplay() {
  const bool linkHealthy = linkEstablished && millis() - lastAckReceived < LINK_TIMEOUT_MS;
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setCursor(8, 10);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("SAFETY LINK");
  tft.setCursor(8, 48);
  tft.setTextColor(latchedState == LatchedState::KILL ? ST77XX_RED : ST77XX_GREEN);
  tft.print("STATE: ");
  tft.println(stateName());
  tft.setCursor(8, 82);
  tft.setTextColor(linkHealthy ? ST77XX_GREEN : ST77XX_RED);
  tft.println(linkHealthy ? "LINK: OK" : "LINK: LOST");
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(8, 116);
  if (rssiAvailable) {
    tft.print("RSSI L: ");
    tft.println(localRssi);
    tft.setCursor(8, 144);
    tft.print("RSSI R: ");
    tft.println(remoteRssi);
  } else {
    tft.println("RSSI: WAITING");
  }
}

void readRadio() {
  while (Serial1.available()) {
    const uint8_t byte = static_cast<uint8_t>(Serial1.read());
    if (mavlinkPacket || byte == 0xFE || byte == 0xFD) {
      processMavlinkByte(byte);
      continue;
    }
    const char c = static_cast<char>(byte);
    // Our radio protocol is ASCII. Ignore other bytes that cannot form a line.
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

bool wasPressed(Button& button) {
  const bool raw = digitalRead(button.inputPin);
  const uint32_t now = millis();

  if (raw != button.rawState) {
    button.rawState = raw;
    button.changedAt = now;
  }

  if (raw != button.stableState && now - button.changedAt >= DEBOUNCE_MS) {
    button.stableState = raw;
    return button.stableState == HIGH;
  }

  return false;
}

void setup() {
  Serial.begin(115200);

  pinMode(RIGHT_HIGH_PIN, OUTPUT);
  digitalWrite(RIGHT_HIGH_PIN, HIGH);
  pinMode(RIGHT_INPUT_PIN, INPUT_PULLDOWN);

  pinMode(LEFT_HIGH_PIN, OUTPUT);
  digitalWrite(LEFT_HIGH_PIN, HIGH);
  pinMode(LEFT_INPUT_PIN, INPUT_PULLDOWN);

  rightButton.rawState = rightButton.stableState = digitalRead(RIGHT_INPUT_PIN);
  leftButton.rawState = leftButton.stableState = digitalRead(LEFT_INPUT_PIN);

  Serial1.setTX(RADIO_TX);
  Serial1.setRX(RADIO_RX);
  Serial1.begin(RADIO_BAUD);

  SPI1.setSCK(LCD_SCK);
  SPI1.setTX(LCD_MOSI);
  SPI1.begin();
  tft.init(170, 320);
  tft.setRotation(1);
  updateDisplay();

  rp2040.wdt_begin(3000);
  Serial.println("Operator transmitter ready");
  Serial.println("Latched state: KILL");

  // Cause the first heartbeat to be sent immediately.
  lastHeartbeatSent = millis() - HEARTBEAT_INTERVAL_MS;
}

void loop() {
  rp2040.wdt_reset();
  readRadio();

  const bool rightPressed = wasPressed(rightButton);
  const bool leftPressed = wasPressed(leftButton);

  // RIGHT is KILL and takes priority if both buttons are pressed together.
  if (rightPressed) {
    sendCommand(LatchedState::KILL);
  } else if (leftPressed) {
    sendCommand(LatchedState::RESET);
  }

  if (millis() - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS) {
    sendStatePacket("HB");
  }

  if (millis() - lastMavlinkHeartbeat >= MAVLINK_HEARTBEAT_INTERVAL_MS) {
    lastMavlinkHeartbeat = millis();
    sendMavlinkHeartbeat();
  }

  const bool linkHealthy = linkEstablished &&
                           millis() - lastAckReceived < LINK_TIMEOUT_MS;
  if (linkHealthy != linkWasHealthy) {
    linkWasHealthy = linkHealthy;
    Serial.println(linkHealthy ? "Receiver link healthy" : "Receiver link lost");
  }

  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}
