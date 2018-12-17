/*
	FILE: 		EnviralDesign.cpp
	VERSION: 	0.0.1
	PURPOSE: 	Configure NeoPixelDriver via UDP
	LICENCE:	GPL v3 (http://www.gnu.org/licenses/gpl.html)
 */

#include "EnviralDesign.h"

EnviralDesign::EnviralDesign(){}

EnviralDesign::EnviralDesign(uint16_t *pixelsPerStrip, uint16_t *chunkSize, uint16_t *maPerPixel, String *deviceName, float *amps, uint16_t *udpPort, byte *InitColor) {
    this->pix = pixelsPerStrip;
    this->ch = chunkSize;
    this->maP = maPerPixel;
    this->dName = deviceName;
    this->a = amps;
    this->uP = udpPort;
    this->iC = InitColor;
}

void EnviralDesign::start() {

#ifdef ESP8266
    EEPROM.begin(512);
#endif
    if (isWriteMode(PIXELS_PER_STRIP_ADDRESS)) {
        uint16_t val = readIntFromAddress(PIXELS_PER_STRIP_ADDRESS);
        if (val <= 1500) *this->pix = val;
    }
    if (isWriteMode(CHUNK_SIZE_ADDRESS)) {
        uint16_t val = readIntFromAddress(CHUNK_SIZE_ADDRESS);
        if (val <= 200) *this->ch = val;
    }            
    if (isWriteMode(MA_PER_PIXEL_ADDRESS)){
        uint16_t val = readIntFromAddress(MA_PER_PIXEL_ADDRESS);
        *this->maP = val;
    }
    if (isWriteMode(NAME_ADDRESS)) {
        String val = readStringFromAddress(NAME_ADDRESS);
        *this->dName = val;
    }
    if (isWriteMode(AMPS)) {        
        float val = readFloatFromAddress(AMPS);
        *this->a = val;
    }
    if (isWriteMode(UDP_PORT)) {
        uint16_t val = readIntFromAddress(UDP_PORT);
        *this->uP = val;
    }
    if (isWriteMode(INIT_COLOR)) {
        this->iC[0] = EEPROM.read(INIT_COLOR + 2);
        this->iC[1] = EEPROM.read(INIT_COLOR + 3);
        this->iC[2] = EEPROM.read(INIT_COLOR + 4);
    }
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::update(uint16_t pixelsPerStrip, uint16_t chunkSize, uint16_t maPerPixel) {
    updatePixelsPerStrip(pixelsPerStrip);
    updateChunkSize(chunkSize);
    updatemaPerPixel(maPerPixel);
}

void EnviralDesign::updatePixelsPerStrip(uint16_t val) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(PIXELS_PER_STRIP_ADDRESS);
    writeIntToAddress(PIXELS_PER_STRIP_ADDRESS, val);
    if (val <= 1500) *this->pix = val;
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::updateChunkSize(uint16_t val) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(CHUNK_SIZE_ADDRESS);
    writeIntToAddress(CHUNK_SIZE_ADDRESS, val);
    *this->ch = val;
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::updatemaPerPixel(uint16_t val) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(MA_PER_PIXEL_ADDRESS);
    writeIntToAddress(MA_PER_PIXEL_ADDRESS, val);
    *this->maP = val;
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::updateDeviceName(String val) {
    if (val.length() >= MAX_NAME_LENGTH) return;
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(NAME_ADDRESS);
    writeStringToAddress(NAME_ADDRESS, val);
    *this->dName = val;
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::updateAmps(float val) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(AMPS);
    writeFloatToAddress(AMPS, val);
    *this->a = val;
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::updateUDPport(uint16_t val) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(UDP_PORT);
    writeIntToAddress(UDP_PORT, val);
    *this->uP = val;
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::updateInitColor(byte val[]) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    updateMode(INIT_COLOR);
    EEPROM.write(INIT_COLOR + 2, val[0]);
    EEPROM.write(INIT_COLOR + 3, val[1]);
    EEPROM.write(INIT_COLOR + 4, val[2]);
    this->iC[0] = val[0];
    this->iC[1] = val[1];
    this->iC[2] = val[2];
#ifdef ESP8266
    EEPROM.end();
#endif
}

void EnviralDesign::writeIntToAddress(uint16_t address, uint16_t value) {
    address += 2;
    UINT16_ARRAY wval;
    wval.num = value;
    EEPROM.write(address, wval.bytes[0]);
    EEPROM.write(address + 1, wval.bytes[1]);
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
    UINT16_ARRAY rval;
    address += 2;
    rval.bytes[0] = EEPROM.read(address);
    rval.bytes[1] = EEPROM.read(address + 1);
    return rval.num;
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
        byte storedtime1 = EEPROM.read(COMPILE_TIME);
        byte storedtime2 = EEPROM.read(COMPILE_TIME + 1);
        EEPROM.write(address, storedtime1);
        EEPROM.write(address + 1, storedtime2);
    }
}

boolean EnviralDesign::isWriteMode(uint16_t address) {
    UINT16_ARRAY storedtime;
    storedtime.bytes[0] = EEPROM.read(COMPILE_TIME);
    storedtime.bytes[1] = EEPROM.read(COMPILE_TIME + 1);
    
    UINT16_ARRAY valtime;
    valtime.bytes[0] = EEPROM.read(address);
    valtime.bytes[1] = EEPROM.read(address + 1);
    
    if (valtime.num == storedtime.num) {
        return true;
    } else {
        return false;
    }
}

void EnviralDesign::setCompile(String cotime) {
  
  char str[16];
  cotime.toCharArray(str, 16);
  char *hr, *mn, *sec;
  hr = strtok(str, " :");
  mn = strtok(NULL, " :");
  sec = strtok(NULL, " :");
  UINT16_ARRAY combinetime;
  combinetime.num = String(hr).toInt() * 1800;
  combinetime.num += String(mn).toInt() * 30;
  combinetime.num += String(sec).toInt() / 2;
  uint16_t storedtime = getStoredTime(COMPILE_TIME);
  if (combinetime.num != storedtime) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    EEPROM.write(COMPILE_TIME, combinetime.bytes[0]);
    EEPROM.write(COMPILE_TIME + 1, combinetime.bytes[1]);
#ifdef ESP8266
    EEPROM.end();
#endif
  }
  
}

uint16_t EnviralDesign::getStoredTime(uint16_t address) {
#ifdef ESP8266
    EEPROM.begin(512);
#endif
    UINT16_ARRAY storedtime;
    storedtime.bytes[0] = EEPROM.read(address);
    storedtime.bytes[1] = EEPROM.read(address + 1);
#ifdef ESP8266
    EEPROM.end();
#endif
    return storedtime.num;
}
// EOF
