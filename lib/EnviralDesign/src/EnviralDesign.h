/*
	FILE: 		EnviralDesign.h
	VERSION: 	0.0.1
	PURPOSE: 	Configure NeoPixelDriver via UDP
	LICENCE:	GPL v3 (http://www.gnu.org/licenses/gpl.html)
 */

#ifndef EnviralDesign_H__
#define EnviralDesign_H__

#include <Arduino.h>
#include <EEPROM.h>

#define WRITEMODE 278
#define PIXELS_PER_STRIP_ADDRESS 0
#define CHUNK_SIZE_ADDRESS 4
#define MA_PER_PIXEL_ADDRESS 8

class EnviralDesign
{
public:
    EnviralDesign(uint16_t *pixelsPerStrip, uint16_t *chunkSize, uint16_t *maPerPixel);
    void update(uint16_t pixelsPerStrip, uint16_t chunkSize, uint16_t maPerPixel);
    void updatePixelsPerStrip(uint16_t pixelsPerStrip);
    void updateChunkSize(uint16_t chunkSize);
    void updatemaPerPixel(uint16_t maPerPixel);
private:
    uint16_t readFromAddress(uint16_t address);
    void writeToAddress(uint16_t address, uint16_t value);
    void updateMode(uint16_t address);
    boolean isWriteMode(uint16_t address);
};
#endif