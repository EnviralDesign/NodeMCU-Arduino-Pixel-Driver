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
#define COMPILE_TIME 0
#define PIXELS_PER_STRIP_ADDRESS 1
#define CHUNK_SIZE_ADDRESS 5
#define MA_PER_PIXEL_ADDRESS 9
#define AMPS 13
#define UDP_PORT 19
#define INIT_COLOR 23
#define NAME_ADDRESS 28

typedef union
{
    float num;
    byte bytes[4];
} FLOAT_ARRAY;

class EnviralDesign
{
public:
    EnviralDesign(uint16_t *pixelsPerStrip, uint16_t *chunkSize, uint16_t *maPerPixel, String *deviceName, float *amps, uint16_t *udpPort, byte *InitColor);
    void update(uint16_t pixelsPerStrip, uint16_t chunkSize, uint16_t maPerPixel);
    void updatePixelsPerStrip(uint16_t pixelsPerStrip);
    void updateChunkSize(uint16_t chunkSize);
    void updatemaPerPixel(uint16_t maPerPixel);
    void updateDeviceName(String deviceName);
    void updateAmps(float amps);
    void updateUDPport(uint16_t udpPort);
    void updateInitColor(byte *InitColor);
private:
    uint16_t readIntFromAddress(uint16_t address);
    float readFloatFromAddress(uint16_t address);
    String readStringFromAddress(uint16_t address);
    void writeIntToAddress(uint16_t address, uint16_t value);
    void writeFloatToAddress(uint16_t address, float value);
    void writeStringToAddress(uint16_t address, String value);
    void updateMode(uint16_t address);
    boolean isWriteMode(uint16_t address);
    void setCompile();
};
#endif