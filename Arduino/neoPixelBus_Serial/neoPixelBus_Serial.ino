// include some libraries
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>

#include <EnviralDesign.h>

// Streaming Poll Opcodes
#define CHUNKIDMIN 0
#define CHUNKIDMAX 99
#define UPDATEFRAME 100
#define POLL 200
#define POLLREPLY 201
#define CONFIG 202
#define NOPACKET -1
#define UDPID "EnviralDesignPxlNode"
#define IDLENGTH 20
#define STREAMING_MIN_FRAME_TIME 33333 // 33333 = 30 fps
#define VARIABLES_LENGTH 15 // sum of bytes for user variables. 2 (pixelsPerStrip) + 2 (chunkSize) + 2 (udpPort) + 4 (ampLimit) + 2 (maPerPixel) + 3 (WarmUpColor)
int opcode;
bool minFrameTimeMet = true;

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

///////////////////// USER DEFINED VARIABLES START HERE /////////////////////////////
// NOTICE: these startup settings, especially pertaining to number of pixels and starting color
// will ensure that your nodeMCU can be powered on and run off of a usb 2.0 port of your computer.

String deviceName = "PxlNode-8266";

// number of physical pixels in the strip.
uint16_t pixelsPerStrip = 64;

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
uint16_t chunkSize = 64;

// Dynamically limit brightness in terms of amperage.
float amps = 1;
uint16_t mAPerPixel = 60;

// Unused but kept for compatibility
uint16_t udpPort = 0;

//Set here the inital RGB color to show on module power up
byte InitColor[] = {200, 75, 10};

///////////////////// USER DEFINED VARIABLES END HERE /////////////////////////////

//Interfaces user defined variables with memory stored in EEPROM
EnviralDesign ed(&pixelsPerStrip, &chunkSize, &mAPerPixel, &deviceName, &amps, &udpPort, InitColor);

#define STREAMING_TIMEOUT 10  //  blank streaming frame after X seconds

// If this is set to 1, a lot of debug data will print to the console.
// Will cause horrible stuttering meant for single frame by frame tests and such.
#define DEBUG_MODE 0 //MDB
#define PACKETDROP_DEBUG_MODE 0

//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 2;
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> *strip;
NeoGamma<NeoGammaTableMethod> colorGamma;

byte * packetBuffer;
RgbColor * ledDataBuffer;
byte r;
byte g;
byte b;

//Set here the inital RGB color to show on module power up
RgbColor InitialColor;
RgbColor LastColor=RgbColor(0,0,0);  //hold the last colour in order to stitch one effect with the following.

// used later for holding values - used to dynamically limit brightness by amperage.
RgbColor prevColor;
uint32_t milliAmpsLimit = amps * 1000;
uint32_t milliAmpsCounter = 0;
byte millisMultiplier = 0;

// Reply buffer, for now hardcoded but this might encompass useful data like dropped packets etc.
byte ReplyBuffer[IDLENGTH + 1 + MAX_NAME_LENGTH + VARIABLES_LENGTH] = {0};
byte counterHolder = 0;

long blankTime;
long processingTime;
long arrivedAt;
long lastStreamingFrame=0;

volatile boolean streaming=true;

uint32_t frames=1;
uint32_t offset=0;

void setup() {
  
  ////////////////// A whole bunch of initialization stuff that prints no matter what.
  Serial.begin(115200);
  if (DEBUG_MODE) {
    Serial.println();
    Serial.println();
    Serial.println(F("Serial started")); 
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

  // Initial full black strip push and init.
  blankTime=micros();

}

void loop() { //main program loop  
  opcode = parseStreamingPoll();
  
  // opcodes between 0 and 99 represent the chunkID
  if (opcode <= CHUNKIDMAX && opcode >= CHUNKIDMIN) {
    playStreaming(opcode);
    
  } else if (opcode == UPDATEFRAME) {
    streamingUpdateFrame();
    
  } else if (opcode == CONFIG) {
    streamingConfigDevice();
    
  } else if (opcode == POLL) {
    streamingSendPollReply();
    
  } else if (opcode == POLLREPLY) {
    //POLLREPLY safe to ignore    
  // Streaming but nothing received check timeout
  } else if (lastStreamingFrame!=0 && millis()-lastStreamingFrame>STREAMING_TIMEOUT*1000) {
      if (DEBUG_MODE) {
        Serial.println(F("Streaming timeout"));
      }
      blankFrame();
      lastStreamingFrame=0;
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
  
  // New frame incoming check time
  if (chunkID == 0 && arrivedAt + STREAMING_MIN_FRAME_TIME < micros()) {
    minFrameTimeMet = true;
  }
  
  if (!minFrameTimeMet) {
    return;
  }
  
  arrivedAt=micros();
  
  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("---Incoming---"));
    Serial.print(F("ChunkID: "));
    Serial.println(chunkID);
  }

  // Figure out what our starting offset is.
  //const uint16_t initialOffset = chunkSize * (action - 1);
  const uint16_t initialOffset = chunkSize * chunkID;
  
  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.print(F("---------: "));
    Serial.print(chunkSize);
    Serial.print(F("   "));
    //Serial.println((action - 1));
    Serial.println(F(""));
    Serial.print(F("Init_offset: "));
    Serial.println(initialOffset);
    Serial.print(F(" ifLessThan: "));
    Serial.println((initialOffset + chunkSize));
  }

  // loop through our recently received packet, and assign the corresponding
  // RGB values to their respective places in the strip.
  //if(action<=MAX_ACTION_BYTE) { //check the ation byte is within limits
  uint16_t led=0;
  for (uint16_t i = IDLENGTH + 1; i < (IDLENGTH + chunkSize*3);) {

    r = packetBuffer[i++];
    g = packetBuffer[i++];
    b = packetBuffer[i++];

    strip->SetPixelColor(initialOffset+led++, colorGamma.Correct(RgbColor(r, g, b))); // this line uses gamma correction
    milliAmpsCounter += (r + g + b); // increment our milliamps counter accordingly for use later.
  }
  

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("Finished For Loop!"));
  }

  // if we're debugging packet drops, modify reply buffer.
  if (PACKETDROP_DEBUG_MODE) {
    //ReplyBuffer[action] = 1;
    ReplyBuffer[chunkID] = 1;
  }

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("--end of packet and stuff--"));
    Serial.println(F(""));
  }
}

bool updatePixels(int val) {
  if (val < 0 || val > 1500) {
    return false;
  } else {
    ed.updatePixelsPerStrip(val);
    // Force sprite to reload file
    spritelastfile = "";
    return true;
  }
}

bool updateChunk(int val) {
  ed.updateChunkSize(val);
  return true;
}

bool updateMA(int val) {
  ed.updatemaPerPixel(val);
  // Force sprite to reload file
  spritelastfile = "";
  return true;
}

bool updateName(String val) {
  ed.updateDeviceName(val);
  return true;
}

bool updateAmps(float val) {
  ed.updateAmps(val);
  // Force sprite to reload file
  spritelastfile = "";
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
    Serial.println(F("Starting NeoPixelBus"));
  }
  if (strip) {
    delete strip;
  }
 
  if (ledDataBuffer) {
    free(ledDataBuffer);
  }
  strip = new NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod>(pixelsPerStrip, PixelPin);
  ledDataBuffer = (RgbColor *)malloc(pixelsPerStrip);
  
  strip->Begin();
}

void setPacketSize() {
  if (packetBuffer) free(packetBuffer);
  // Max packet size is the IDLENGTH + OPCODE + ( RGB[chunksize][3] OR Update size )
  // Update size MAX_NAME_LENGTH + sizeof(PixelsPerStrip, ChunkSize, UdpPort, AmpsLimit, MaPerPixel, WarmUpColor)
  udpPacketSize = ( IDLENGTH + 1 + max( (chunkSize*3), (MAX_NAME_LENGTH  + VARIABLES_LENGTH) ) );
  packetBuffer = (byte *)malloc(udpPacketSize);//buffer to hold incoming and outgoing packets
}

void initDisplay() {  
  milliAmpsCounter = 0;
  millisMultiplier = 0;
  InitialColor=RgbColor(InitColor[0],InitColor[1],InitColor[2]); 
  InitialColor=adjustToMaxMilliAmps(InitialColor);
  for(int i=0;i<=90;i++) {
    paintFrame(RgbColor(InitialColor.R*i/90.0,InitialColor.G*i/90.0,InitialColor.B*i/90.0));
    strip->Show();
    delay(16);
  };
}

bool compareArrays(uint8_t arr1[], uint8_t arr2[], uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (arr1[i] != arr2[i]) {
      return false;
    }
  }
  return true;
}

// Examines the first 20 bytes of a udp packet to determine if it matches 'EnviralDesignPxlNode'
// Returns the opcode
int parseUdpPoll() {

  int packetSize = udp.parsePacket();
  
  if (!packetSize) {
    return NOPACKET;
  }

  udp.read(packetBuffer, udpPacketSize);

  int i = 0;
  
  for (; i < IDLENGTH; i++) {
    if (packetBuffer[i] != UDPID[i]) {
      if (DEBUG_MODE) {
        Serial.println(F("Mismatch"));
      }
      return NOPACKET;
    }
  } 
  if (PACKETDROP_DEBUG_MODE) {
    Serial.print(F("Matched ")); Serial.println(packetBuffer[i]);
  }
  return packetBuffer[i];
}

void udpUpdateFrame() {

  if (!minFrameTimeMet) {
    milliAmpsCounter = 0; // reset the milliAmpsCounter for the next frame.
    return;
  }

  //Reset time boolean
  minFrameTimeMet = false;

  if (PACKETDROP_DEBUG_MODE) {
    Serial.println("Updating Frame");
  }
  pinMode(BUILTIN_LED, OUTPUT);

  // this math gets our sum total of r/g/b vals down to milliamps (~60mA per pixel)
  milliAmpsCounter /= 13;
  //framesMD[frameIndex].power=milliAmpsCounter;

  // because the Darken function uses a value from 0-255 this next line maths it into the right range and type.
  millisMultiplier = 255 - (byte)( constrain( ((float)milliAmpsLimit / (float)milliAmpsCounter), 0, 1 ) * 256);
  millisMultiplier = map(millisMultiplier, 0, 255, 255, 0); // inverse the multiplier to work with new brightness control method
  // Collect data  MDB
  //framesMD[frameIndex].adjustedPower=millisMultiplier;

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("Trying to update leds..."));
    Serial.print(F("Dimming leds to: "));
    Serial.println( millisMultiplier );
  }


  // We already applied our r/g/b values to the strip, but we haven't updated it yet.
  // Since we needed the sum total of r/g/b values to calculate brightness, we
  // can loop through all the values again now that we have the right numbers
  // and scale brightness if we need to.
  
  if(millisMultiplier!=255) { //dim LEDs only if required
    strip->SetBrightness(millisMultiplier); // this new brightness control method was added to lib recently, affects entire strip at once.
  }
  strip->Show();   // write all the pixels out
  lastStreamingFrame=millis();
  milliAmpsCounter = 0; // reset the milliAmpsCounter for the next frame.

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("Finished updating Leds!"));
  }

  // if we're debugging packet drops, modify reply buffer.
  if (PACKETDROP_DEBUG_MODE) {
    // set the last byte of the reply buffer to 2, indiciating that the frame was sent to leds.
    ReplyBuffer[sizeof(ReplyBuffer) - 1] = 2;
    ReplyBuffer[0] = counterHolder;
    counterHolder += 1;
    // write out the response packet back to sender!
    udp.beginPacket(udp.remoteIP(), UDP_PORT_OUT);
    // clear the response buffer string.
    for (byte i = 0; i < sizeof(ReplyBuffer); i++) {
      udp.write(ReplyBuffer[i]);
      ReplyBuffer[i] = 0;
    }
    udp.endPacket();
  }

  pinMode(BUILTIN_LED, INPUT);
}

void udpConfigDevice() {
  // Set packetbuffer index past the ID and OpCode bytes
  int i = IDLENGTH + 1;
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
  
}
 
void udpSendPollReply() {
  int i = 0;
  for (; i < IDLENGTH; i++) {
    ReplyBuffer[i] = UDPID[i];
  }
  // Set opcode to POLLREPLY
  ReplyBuffer[i++] = POLLREPLY;

  //Copy device name to reply buffer
  for (int j = 0; j < MAX_NAME_LENGTH; j++) {
    ReplyBuffer[i++] = deviceName[j];
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
  ReplyBuffer[i] = '\0';
  if (PACKETDROP_DEBUG_MODE) {
    Serial.print(F("Sending message to "));Serial.println(udp.remoteIP());
    Serial.println(F("Contents: "));
    for (i = 0; i < sizeof(ReplyBuffer); i++) {
      Serial.print(ReplyBuffer[i]);Serial.print(F(":"));
      Serial.print((char)ReplyBuffer[i]);Serial.print(F(" "));
    }
    Serial.println(F("EndReplyBuffer"));
  }
  udp.beginPacket(udp.remoteIP(), UDP_PORT_OUT);
  // clear the response buffer string.
  for (i = 0; i < sizeof(ReplyBuffer); i++) {
    udp.write(ReplyBuffer[i]);
    ReplyBuffer[i] = 0;
  }
  udp.endPacket();
}
