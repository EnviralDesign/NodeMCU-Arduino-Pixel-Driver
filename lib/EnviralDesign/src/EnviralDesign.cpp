/*
	FILE: 		EnviralDesign.cpp
	VERSION: 	0.0.1
	PURPOSE: 	Configure NeoPixelDriver via UDP
	LICENCE:	GPL v3 (http://www.gnu.org/licenses/gpl.html)
 */

#include "EnviralDesign.h"

EnviralDesign::EnviralDesign(uint16_t *pixelsPerStrip, uint16_t *chunkSize, uint16_t *maPerPixel) {
	
	EEPROM.begin(512);
	if (isWriteMode(PIXELS_PER_STRIP_ADDRESS))
        *pixelsPerStrip = readFromAddress(PIXELS_PER_STRIP_ADDRESS);
    if (isWriteMode(CHUNK_SIZE_ADDRESS))
        *chunkSize = readFromAddress(CHUNK_SIZE_ADDRESS);
    if (isWriteMode(MA_PER_PIXEL_ADDRESS))
        *maPerPixel = readFromAddress(MA_PER_PIXEL_ADDRESS);
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
    writeToAddress(PIXELS_PER_STRIP_ADDRESS, pixelsPerStrip);
    EEPROM.end();
}

void EnviralDesign::updateChunkSize(uint16_t chunkSize) {
    EEPROM.begin(512);
    updateMode(CHUNK_SIZE_ADDRESS);
    writeToAddress(CHUNK_SIZE_ADDRESS, chunkSize);
    EEPROM.end();
}

void EnviralDesign::updatemaPerPixel(uint16_t maPerPixel) {
    EEPROM.begin(512);
    updateMode(MA_PER_PIXEL_ADDRESS);
    writeToAddress(MA_PER_PIXEL_ADDRESS, maPerPixel);
    EEPROM.end();
}

void EnviralDesign::writeToAddress(uint16_t address, uint16_t value) {
    if (value > 255) {
        EEPROM.write(address, 255);
        EEPROM.write(address + 1, value - 255);
    } else {
        EEPROM.write(address, value);
        EEPROM.write(address + 1, 0);
    }    
}

uint16_t EnviralDesign::readFromAddress(uint16_t address) {
    uint16_t value = EEPROM.read(address);
    value = value + EEPROM.read(address + 1);
	return value;
}

void EnviralDesign::updateMode(uint16_t address) {
    if (!isWriteMode(address)) {
        EEPROM.write(address + 2, WRITEMODE / 2);
        EEPROM.write(address + 3, WRITEMODE / 2);
    }
}

boolean EnviralDesign::isWriteMode(uint16_t address) {
    byte wmode1 = EEPROM.read(address + 2);
    byte wmode2 = EEPROM.read(address + 3);
    uint16_t wmode = wmode1 + wmode2;
    return (wmode == WRITEMODE && wmode1 == wmode2);
}
// EOF
