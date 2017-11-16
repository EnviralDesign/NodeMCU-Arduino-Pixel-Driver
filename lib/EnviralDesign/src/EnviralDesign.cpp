/*
	FILE: 		EnviralDesign.cpp
	VERSION: 	0.0.1
	PURPOSE: 	Configure NeoPixelDriver via UDP
	LICENCE:	GPL v3 (http://www.gnu.org/licenses/gpl.html)
 */

#include "EnviralDesign.h"

EnviralDesign::EnviralDesign(uint16_t *pixelsPerStrip, uint16_t *chunkSize, uint16_t *maPerPixel, String *deviceName, float *amps, uint16_t *udpPort, byte *InitColor) {
	
	EEPROM.begin(512);
    setCompile();
    if (isWriteMode(PIXELS_PER_STRIP_ADDRESS))
        *pixelsPerStrip = readIntFromAddress(PIXELS_PER_STRIP_ADDRESS);
    if (isWriteMode(CHUNK_SIZE_ADDRESS))
        *chunkSize = readIntFromAddress(CHUNK_SIZE_ADDRESS);
    if (isWriteMode(MA_PER_PIXEL_ADDRESS))
        *maPerPixel = readIntFromAddress(MA_PER_PIXEL_ADDRESS);
    if (isWriteMode(NAME_ADDRESS))
        *deviceName = readStringFromAddress(NAME_ADDRESS);
    if (isWriteMode(AMPS))
        *amps = readFloatFromAddress(AMPS);
    if (isWriteMode(UDP_PORT))
        *udpPort = readIntFromAddress(UDP_PORT);
    if (isWriteMode(INIT_COLOR)) {
        InitColor[0] = EEPROM.read(INIT_COLOR + 2);
        InitColor[0] = EEPROM.read(INIT_COLOR + 3);
        InitColor[0] = EEPROM.read(INIT_COLOR + 4);
    }
    EEPROM.end();
}

void EnviralDesign::update(uint16_t pixelsPerStrip, uint16_t chunkSize, uint16_t maPerPixel) {
    updatePixelsPerStrip(pixelsPerStrip);
    updateChunkSize(chunkSize);
    updatemaPerPixel(maPerPixel);
}

void EnviralDesign::updatePixelsPerStrip(uint16_t pixelsPerStrip) {
    EEPROM.begin(512);
    updateMode(PIXELS_PER_STRIP_ADDRESS);
    writeIntToAddress(PIXELS_PER_STRIP_ADDRESS, pixelsPerStrip);
    EEPROM.end();
}

void EnviralDesign::updateChunkSize(uint16_t chunkSize) {
    EEPROM.begin(512);
    updateMode(CHUNK_SIZE_ADDRESS);
    writeIntToAddress(CHUNK_SIZE_ADDRESS, chunkSize);
    EEPROM.end();
}

void EnviralDesign::updatemaPerPixel(uint16_t maPerPixel) {
    EEPROM.begin(512);
    updateMode(MA_PER_PIXEL_ADDRESS);
    writeIntToAddress(MA_PER_PIXEL_ADDRESS, maPerPixel);
    EEPROM.end();
}

void EnviralDesign::updateDeviceName(String deviceName) {
    if (deviceName.length() >= 64) return;
    EEPROM.begin(512);
    updateMode(NAME_ADDRESS);
    writeStringToAddress(NAME_ADDRESS, deviceName);
    EEPROM.end();
}

void EnviralDesign::updateAmps(float amps) {
    EEPROM.begin(512);
    updateMode(AMPS);
    writeFloatToAddress(AMPS, amps);
    EEPROM.end();
}

void EnviralDesign::updateUDPport(uint16_t udpPort) {
    EEPROM.begin(512);
    updateMode(UDP_PORT);
    writeIntToAddress(UDP_PORT, udpPort);
    EEPROM.end();
}

void EnviralDesign::updateInitColor(byte *InitColor) {
    EEPROM.begin(512);
    updateMode(INIT_COLOR);
    EEPROM.write(INIT_COLOR + 2, InitColor[0]);
    EEPROM.write(INIT_COLOR + 3, InitColor[1]);
    EEPROM.write(INIT_COLOR + 4, InitColor[2]);
    EEPROM.end();
}

void EnviralDesign::writeIntToAddress(uint16_t address, uint16_t value) {
    address += 2;
    if (value > 255) {
        EEPROM.write(address, 255);
        EEPROM.write(address + 1, value - 255);
    } else {
        EEPROM.write(address, value);
        EEPROM.write(address + 1, 0);
    }
}

void EnviralDesign::writeFloatToAddress(uint16_t address, float value) {
    address += 2;
    FLOAT_ARRAY wval;
    wval.num = value;
    EEPROM.write(address, wval.bytes[0]);
    EEPROM.write(address + 1, wval.bytes[1]);
    EEPROM.write(address + 2, wval.bytes[2]);
    EEPROM.write(address + 3, wval.bytes[3]);
}

void EnviralDesign::writeStringToAddress(uint16_t address, String value) {
    address += 2;
    char buf[64];
    value.toCharArray(buf, 64);
    for (byte i = 0; i < 64; i++) {
        EEPROM.write(address + i, buf[i]);
        if (buf[i] == '\0') break;
    }
}

uint16_t EnviralDesign::readIntFromAddress(uint16_t address) {
    uint16_t value = EEPROM.read(address + 2);
    value = value + EEPROM.read(address + 3);
	return value;
}

float EnviralDesign::readFloatFromAddress(uint16_t address) {
    FLOAT_ARRAY rval;
    address += 2;
    rval.bytes[0] = EEPROM.read(address);
    rval.bytes[1] = EEPROM.read(address + 1);
    rval.bytes[2] = EEPROM.read(address + 2);
    rval.bytes[3] = EEPROM.read(address + 3);
	return rval.num;
}

String EnviralDesign::readStringFromAddress(uint16_t address) {
    address += 2;
    char buf[64];
    for (byte i = 0; i < 64; i++) {
        buf[i] = EEPROM.read(address + i);
        if (buf[i] == '\0') break;
    }
    String value(buf);    
	return value;
}

void EnviralDesign::updateMode(uint16_t address) {
    if (!isWriteMode(address)) {
        EEPROM.write(address, WRITEMODE / 2);
        EEPROM.write(address + 1, WRITEMODE / 2);
    }
}

boolean EnviralDesign::isWriteMode(uint16_t address) {
    byte wmode1 = EEPROM.read(address);
    byte wmode2 = EEPROM.read(address + 1);
    uint16_t wmode = wmode1 + wmode2;
    return (wmode == WRITEMODE && wmode1 == wmode2);
}

void EnviralDesign::setCompile() {
  String ctime = __TIME__;
  byte combinetime = ctime.substring(0,2).toInt();
  combinetime = combinetime + ctime.substring(3,5).toInt();
  combinetime = combinetime + ctime.substring(6).toInt();
  byte storedtime = EEPROM.read(COMPILE_TIME);
  if (combinetime != storedtime) {
      EEPROM.write(COMPILE_TIME, combinetime);
      EEPROM.write(PIXELS_PER_STRIP_ADDRESS, 0);
      EEPROM.write(PIXELS_PER_STRIP_ADDRESS + 1, 0);
      EEPROM.write(CHUNK_SIZE_ADDRESS, 0);
      EEPROM.write(CHUNK_SIZE_ADDRESS + 1, 0);
      EEPROM.write(MA_PER_PIXEL_ADDRESS, 0);
      EEPROM.write(MA_PER_PIXEL_ADDRESS + 1, 0);
      EEPROM.write(AMPS, 0);
      EEPROM.write(AMPS + 1, 0);
      EEPROM.write(UDP_PORT, 0);
      EEPROM.write(UDP_PORT + 1, 0);
      EEPROM.write(INIT_COLOR, 0);
      EEPROM.write(INIT_COLOR + 1, 0);
      EEPROM.write(NAME_ADDRESS, 0);
      EEPROM.write(NAME_ADDRESS + 1, 0);
  }
}
// EOF
