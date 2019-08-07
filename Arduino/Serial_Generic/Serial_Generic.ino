// include some libraries
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>

#include "EnviralDesign.h"

//Change which Serial port the device listens for commands and outputs debugging info.
#define DEBUG_PORT Serial
#define INPUT_PORT Serial

// Streaming Poll Opcodes
#define CHUNKIDMIN 0
#define CHUNKIDMAX 99
#define UPDATEFRAME 100
#define POLL 200
#define POLLREPLY 201
#define CONFIG 202
#define NOPACKET -1
#define VARIABLES_LENGTH 15 // sum of bytes for user variables. 2 (pixelsPerStrip) + 2 (chunkSize) + 2 (udpPort) + 4 (ampLimit) + 2 (maPerPixel) + 3 (WarmUpColor)
int opcode;

// Stream packet protocol
#define SERIAL_TIMEOUT 200 // wait for next byte in stream when decoding a high byte

// Holds data from serial
uint8_t * packetBuffer;

///////////////////// USER DEFINED VARIABLES START HERE /////////////////////////////
// NOTICE: these startup settings, especially pertaining to number of pixels and starting color
// will ensure that your nodeMCU can be powered on and run off of a usb 2.0 port of your computer.

String deviceName = "PxlNode-Serial";

// number of physical pixels in the strip.
uint16_t pixelsPerStrip = 64;

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
uint16_t chunkSize = 64;

// Dynamically limit brightness in terms of amperage.
float amps = 50;
uint16_t mAPerPixel = 60;

// Unused but kept for compatibility
uint16_t udpPort = 0;

//Set here the inital RGB color to show on module power up
byte InitColor[] = {200, 75, 10};

///////////////////// USER DEFINED VARIABLES END HERE /////////////////////////////

//Interfaces user defined variables with memory stored in EEPROM
EnviralDesign ed(&pixelsPerStrip, &chunkSize, &mAPerPixel, &deviceName, &amps, &udpPort, InitColor);

#define STREAMING_TIMEOUT 10000  //  blank streaming frame after X seconds

// If this is set to 1, a lot of debug data will print to the console.
// Will cause horrible stuttering meant for single frame by frame tests and such.
#define DEBUG_MODE 1 //MDB
#define PACKETDROP_DEBUG_MODE 0

//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 2;

NeoPixelBrightnessBus <NeoGrbFeature, Neo800KbpsMethod> *strip;
NeoGamma<NeoGammaTableMethod> colorGamma;

//used to dynamically limit brightness by amperage.
uint32_t milliAmpsLimit;

// Reply buffer, for now hardcoded but this might encompass useful data like dropped packets etc.
byte ReplyBuffer[1 + MAX_NAME_LENGTH + VARIABLES_LENGTH] = {0};
byte counterHolder = 0;

unsigned long lastStreamingFrame=0;

void setup() {
  
  ////////////////// A whole bunch of initialization stuff that prints no matter what.
  if (DEBUG_MODE || PACKETDROP_DEBUG_MODE && INPUT_PORT != DEBUG_PORT) {
    DEBUG_PORT.begin(115200);
  }
  INPUT_PORT.begin(115200);
  INPUT_PORT.setTimeout(SERIAL_TIMEOUT);
  delay(10);
  if (DEBUG_MODE) {
    DEBUG_PORT.println();
    DEBUG_PORT.println();
    DEBUG_PORT.println(F("Serial started"));
    DEBUG_PORT.flush();
    delay(100);
  }
  ed.setCompile(String(__TIME__));    //Compiling erases variables previously changed over the network
  ed.start(); 
  
  //Initializes NeoPixelBus
  startNeoPixelBus();
  
  //Sets the size of the Serial packets
  setPacketSize();

  //Animate from dark to initial color in 3 seconds on module power up
  initDisplay();

  // Set milliamps value
  milliAmpsLimit = amps * 1000;

}

void loop() { //main program loop  
  
  opcode = getSerialData();
  
  // opcodes between 0 and 99 represent the chunkID
  if (opcode <= CHUNKIDMAX && opcode >= CHUNKIDMIN) {
    playStreaming(opcode);
    
  } else if (opcode == UPDATEFRAME) {
    serialUpdateFrame();
    
  } else if (opcode == CONFIG) {
    serialConfigDevice();
    
  } else if (opcode == POLL) {
    serialSendPollReply();
    
  } else if (opcode == POLLREPLY) {
    //POLLREPLY safe to ignore    
  // Streaming but nothing received check timeout
  } else if (lastStreamingFrame!=0 && millis()-lastStreamingFrame>STREAMING_TIMEOUT*1000) {
      if (DEBUG_MODE) {
        DEBUG_PORT.println(F("Streaming timeout"));
      }
      blankFrame();
      blankPacket();
      lastStreamingFrame=0;
  }
}

int getSerialData() {

  if ( INPUT_PORT.available() > 0) {
    unsigned long packetBuildStart;

    int opcode_found = INPUT_PORT.read();

    if (opcode_found <= CHUNKIDMAX && opcode_found >= CHUNKIDMIN) {

      size_t num_read = INPUT_PORT.readBytes(packetBuffer, chunkSize * 3);
      if (DEBUG_MODE) {
        DEBUG_PORT.print(F("Bytes read "));DEBUG_PORT.println(num_read);
      }
      
    } else if (opcode_found == UPDATEFRAME) {

      //Do nothing
    
    } else if (opcode_found == CONFIG) {

      size_t num_read = INPUT_PORT.readBytes(packetBuffer, MAX_NAME_LENGTH + VARIABLES_LENGTH);
      if (DEBUG_MODE) {
        DEBUG_PORT.print(F("Bytes read "));DEBUG_PORT.println(num_read);
      }
    
    } else if (opcode_found == POLL) {

      // Do nothing
    
    } else if (opcode_found == POLLREPLY) {

      // Nothing to do with the reply so dump it
      while ( INPUT_PORT.available() > 0 ) {
        INPUT_PORT.read();
      }
    
    // Unrecognized opcode
    } else {
      
      if (DEBUG_MODE) {
        DEBUG_PORT.println(F("Unrecognized opcode"));
      }

      while ( INPUT_PORT.available() > 0 ) {
        INPUT_PORT.read();
      }
      
      opcode_found = NOPACKET;
    }

    return opcode_found;

  } else {
    return NOPACKET;
  }
}

uint16_t getPacketSize() {
  return ( max( (chunkSize*3), (MAX_NAME_LENGTH  + VARIABLES_LENGTH) ) );
}

// Max packet size is the OPCODE + ( RGB[chunksize][3] OR Update size )
// Update size MAX_NAME_LENGTH + sizeof(PixelsPerStrip, ChunkSize, UdpPort, AmpsLimit, MaPerPixel, WarmUpColor)
void blankPacket() {
  uint16_t packetSize = getPacketSize();
  for (uint16_t i = 0; i < packetSize; i++) {
    packetBuffer[i] = 0;
  }
}

void blankFrame() {
  paintFrame(RgbColor(0,0,0));
  strip->Show();
};

void paintFrame(RgbColor c) {
  c=adjustToMaxMilliAmps(c); // do not allow to exceed max current
  for (uint16_t i = 0; i < pixelsPerStrip; i++) strip->SetPixelColor(i, c);
};

RgbColor adjustToMaxMilliAmps(RgbColor c) {
  float ma= (mAPerPixel/3) * (c.R+c.G+c.B) /255.0 * pixelsPerStrip;//float ma=20*(c.R+c.G+c.B)/255.0*pixelsPerStrip;
  RgbColor r=c;
  if (ma > milliAmpsLimit)  {// need to adjust down
    r.R=c.R*milliAmpsLimit/ma;
    r.G=c.G*milliAmpsLimit/ma;
    r.B=c.B*milliAmpsLimit/ma;
  }

  return r;
}; 

HsbColor adjustToMaxMilliAmps(HsbColor c) {
  float ma = (float)mAPerPixel * c.B * pixelsPerStrip;
  HsbColor r = c;
  if (ma > milliAmpsLimit) {
    r.B = c.B * milliAmpsLimit/ma;
  }
  return r;
}

HslColor adjustToMaxMilliAmps(HslColor c) {
  float ma = (float)mAPerPixel * c.L * pixelsPerStrip;
  HslColor r = c;
  if (ma > milliAmpsLimit) {
    r.L = c.L * milliAmpsLimit/ma;
  }
  return r;
}

void playStreaming(int chunkID) {
  
  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    DEBUG_PORT.println(F("---Incoming---"));
    DEBUG_PORT.print(F("ChunkID: "));
    DEBUG_PORT.println(chunkID);
  }

  // Figure out what our starting offset is.
  //const uint16_t initialOffset = chunkSize * (action - 1);
  const uint16_t initialOffset = chunkSize * chunkID;
  
  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    DEBUG_PORT.print(F("---------: "));
    DEBUG_PORT.print(chunkSize);
    DEBUG_PORT.print(F("   "));
    DEBUG_PORT.println(F(""));
    DEBUG_PORT.print(F("Init_offset: "));
    DEBUG_PORT.println(initialOffset);
    DEBUG_PORT.print(F(" ifLessThan: "));
    DEBUG_PORT.println((initialOffset + chunkSize));
  }

  // loop through our recently received packet, and assign the corresponding
  // RGB values to their respective places in the strip.
  uint16_t index=initialOffset;
  byte r;
  byte g;
  byte b;
  for (uint32_t i = 0; i < chunkSize*3;) {

    r = packetBuffer[i++];
    g = packetBuffer[i++];
    b = packetBuffer[i++];

    strip->SetPixelColor(index++, colorGamma.Correct(RgbColor(r, g, b))); // this line uses gamma correction
    if (index >= pixelsPerStrip) {
      break;
    }
  }  

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    DEBUG_PORT.println(F("Finished For Loop!"));
  }

  // if we're debugging packet drops, modify reply buffer.
  if (PACKETDROP_DEBUG_MODE) {
    ReplyBuffer[chunkID] = 1;
  }

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    DEBUG_PORT.println(F("--end of packet and stuff--"));
    DEBUG_PORT.println(F(""));
  }
}

bool updatePixels(int val) {
  if (val < 0 || val > 1500) {
    return false;
  } else {
    ed.updatePixelsPerStrip(val);

    return true;
  }
}

bool updateChunk(int val) {
  ed.updateChunkSize(val);
  return true;
}

bool updateMA(int val) {
  ed.updatemaPerPixel(val);

  return true;
}

bool updateName(String val) {
  ed.updateDeviceName(val);
  return true;
}

bool updateAmps(float val) {
  ed.updateAmps(val);
  
  // Update milliamps value
  milliAmpsLimit = amps * 1000;
  return true;
}

bool updateUDP(int val) {
  ed.updateUDPport(val);
  return true;
}

bool updateWarmUp(byte v1, byte v2, byte v3) {
  byte parameters[3] = {v1, v2, v3};
  ed.updateInitColor(parameters);
  return true;
}

void startNeoPixelBus() {
  if (DEBUG_MODE) {
    DEBUG_PORT.println(F("Starting NeoPixelBus"));
  }
  if (strip) {
    delete strip;
  }
 
  strip = new NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod>(pixelsPerStrip, PixelPin);
  
  strip->Begin();
}

void setPacketSize() {
  if (packetBuffer) {
    delete packetBuffer;
  }
  // Max packet size is the OPCODE + ( RGB[chunksize][3] OR Update size )
  // Update size MAX_NAME_LENGTH + sizeof(PixelsPerStrip, ChunkSize, UdpPort, AmpsLimit, MaPerPixel, WarmUpColor)
  uint16_t packetSize = getPacketSize();
  packetBuffer = (uint8_t *)malloc(packetSize);//buffer to hold incoming packets
}

void initDisplay() {
  if (DEBUG_MODE) {
    DEBUG_PORT.println(F("Initializing display"));
  }
  RgbColor InitialColor=adjustToMaxMilliAmps(RgbColor(InitColor[0],InitColor[1],InitColor[2]));
  for(int i=0;i<=90;i++) {
    paintFrame(RgbColor(InitialColor.R*i/90.0,InitialColor.G*i/90.0,InitialColor.B*i/90.0));
    strip->Show();
    delay(16);
  };
}

void serialUpdateFrame() {

  if (PACKETDROP_DEBUG_MODE) {
    DEBUG_PORT.println("Updating Frame");
  }

  // this math gets our sum total of r/g/b vals down to milliamps (~60mA per pixel)
  uint32_t milliAmpsCounter = 0;
  uint8_t *pixelBuf = strip->Pixels();
  uint32_t pixelSize = strip->PixelsSize();
  double conversion = (mAPerPixel / 3.0) / 255.0;
  for (uint32_t i = 0; i < pixelSize; i++) {
    milliAmpsCounter += pixelBuf[i];
  }

  if (PACKETDROP_DEBUG_MODE) {
    DEBUG_PORT.print(F("Raw rgb values for mA limiter: "));DEBUG_PORT.println(milliAmpsCounter);
    DEBUG_PORT.print(F("Conversion: "));DEBUG_PORT.println(conversion);
    DEBUG_PORT.print(F("PixelSize: "));DEBUG_PORT.println(pixelSize);
  }

  milliAmpsCounter = floor((float)milliAmpsCounter * conversion);

  if (PACKETDROP_DEBUG_MODE) {
    DEBUG_PORT.print(F("Converted mA value: "));DEBUG_PORT.println(milliAmpsCounter);
  }
  
  // because the Darken function uses a value from 0-255 this next line maths it into the right range and type.
  byte millisMultiplier = (byte)( constrain( ((float)milliAmpsLimit / (float)milliAmpsCounter), 0, 1 ) * 255);
  //millisMultiplier = map(millisMultiplier, 0, 255, 255, 0); // inverse the multiplier to work with new brightness control method
  // Collect data  MDB
  //framesMD[frameIndex].adjustedPower=millisMultiplier;

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    DEBUG_PORT.println(F("Trying to update leds..."));
    DEBUG_PORT.print(F("Dimming leds to: "));
    DEBUG_PORT.println( millisMultiplier );
  }

  // We already applied our r/g/b values to the strip, but we haven't updated it yet.
  // Since we needed the sum total of r/g/b values to calculate brightness, we
  // can loop through all the values again now that we have the right numbers
  // and scale brightness if we need to.  
  if(millisMultiplier!=255) { //dim LEDs only if required
    strip->SetBrightness(millisMultiplier); // this new brightness control method was added to lib recently, affects entire strip at once.
  }
  strip->Show();   // write all the pixels out
  strip->SetBrightness(255);
  lastStreamingFrame=millis();

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    DEBUG_PORT.println(F("Finished updating Leds!"));
  }

  // if we're debugging packet drops, modify reply buffer.
  if (PACKETDROP_DEBUG_MODE) {
    // set the last byte of the reply buffer to 2, indiciating that the frame was sent to leds.
    ReplyBuffer[sizeof(ReplyBuffer) - 1] = 2;
    ReplyBuffer[0] = counterHolder;
    counterHolder += 1;

    // clear the response buffer string.
    for (byte i = 0; i < sizeof(ReplyBuffer); i++) {
      INPUT_PORT.write(ReplyBuffer[i]);
      ReplyBuffer[i] = 0;
    }
  }

}

void serialConfigDevice() {
  int i = 0;
  // Get the device name and save it to a buffer
  char nameBuf[MAX_NAME_LENGTH];
  for (int j = 0; j < MAX_NAME_LENGTH; j++) {
    nameBuf[j] = packetBuffer[i++];
  }
  nameBuf[MAX_NAME_LENGTH - 1] = '\0';
  updateName(String(nameBuf));
  byte valBuf[3];
  
  valBuf[0] = packetBuffer[i++];
  valBuf[1] = packetBuffer[i++];
  updatePixels(valBuf[0] * 256 + valBuf[1]);
  
  valBuf[0] = packetBuffer[i++];
  valBuf[1] = packetBuffer[i++];
  updateChunk(valBuf[0] * 256 + valBuf[1]);
  
  valBuf[0] = packetBuffer[i++];
  valBuf[1] = packetBuffer[i++];
  updateUDP(valBuf[0] * 256 + valBuf[1]);
  
  FLOAT_ARRAY tempF;
  for (int j = 0; j < 4; j++) {
    tempF.bytes[j] = packetBuffer[i++];
  }
  updateAmps(tempF.num);
  
  valBuf[0] = packetBuffer[i++];
  valBuf[1] = packetBuffer[i++];
  updateMA(valBuf[0] * 256 + valBuf[1]);
  
  valBuf[0] = packetBuffer[i++];
  valBuf[1] = packetBuffer[i++];
  valBuf[2] = packetBuffer[i++];
  updateWarmUp(valBuf[0], valBuf[1], valBuf[2]);

  //Initializes NeoPixelBus
  startNeoPixelBus();
  
  //Sets the size of the Serial packets
  setPacketSize();

  //Animate from dark to initial color in 3 seconds on module power up
  initDisplay();

  // Set milliamps value
  milliAmpsLimit = amps * 1000;
  
}
 
void serialSendPollReply() {
  int i = 0;

  // Set opcode to POLLREPLY
  ReplyBuffer[i++] = POLLREPLY;

  //Copy device name to reply buffer
  for (int j = 0; j < MAX_NAME_LENGTH; j++) {
    if (j < deviceName.length()) {
      ReplyBuffer[i++] = deviceName[j];
    } else {
      ReplyBuffer[i++] = '\0';
    }
  }
  
  //Copy pixelsPerStrip value
  ReplyBuffer[i++] = highByte(pixelsPerStrip);
  ReplyBuffer[i++] = lowByte(pixelsPerStrip);

  //Copy chunkSize value
  ReplyBuffer[i++] = highByte(chunkSize);
  ReplyBuffer[i++] = lowByte(chunkSize);

  //Copy udpPort value
  ReplyBuffer[i++] = highByte(udpPort);
  ReplyBuffer[i++] = lowByte(udpPort);

  //Copy amp limit value
  FLOAT_ARRAY tempf;
  tempf.num = amps;
  for (int j = 0; j < 4; j++) {
    ReplyBuffer[i++] = tempf.bytes[j];
  }

  //Copy the ma per pixel
  ReplyBuffer[i++] = highByte(mAPerPixel);
  ReplyBuffer[i++] = lowByte(mAPerPixel);

  //Copy the warmup color
  ReplyBuffer[i++] = InitColor[0];
  ReplyBuffer[i++] = InitColor[1];
  ReplyBuffer[i++] = InitColor[2];
  if (PACKETDROP_DEBUG_MODE) {
    DEBUG_PORT.println(F("Replying..."));
  }

  INPUT_PORT.write(ReplyBuffer, sizeof(ReplyBuffer));

  if (PACKETDROP_DEBUG_MODE) {
    DEBUG_PORT.println(F("EndReplyBuffer"));
  }
}
