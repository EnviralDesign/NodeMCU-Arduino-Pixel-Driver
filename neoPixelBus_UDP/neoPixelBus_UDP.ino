// include some libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h> //MDB
#include <WiFiUdp.h>
#include <NeoPixelBus.h>

// I have not tried more than 512 succesfully at 60 fps
// but I get glitching and stuttering and not sure where the bottleneck is exactly.
// at 30 fps I can go past this number succesfully though.
#define PIXELS_PER_STRIP 512 //MDB

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
#define CHUNK_SIZE 128

// Dynamically limit brightness in terms of amperage.
#define AMPS 4


#define UDP_PORT 2390
#define UDP_PORT_OUT 2391


// NETWORK_HOME
IPAddress local_ip(10, 10, 10, 201);//MDB
IPAddress gateway(10, 10, 10, 254); //MDB
IPAddress subnet(255, 255, 255, 0);
char ssid[] = "bill_wi_the_science_fi_24";  //  your network SSID (name) MDB
char pass[] = "8177937134";       // your network password MDB



// If this is set to 1, a lot of debug data will print to the console.
// Will cause horrible stuttering meant for single frame by frame tests and such.
#define DEBUG_MODE 0 //MDB
#define PACKETDROP_DEBUG_MODE 0


//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 2;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELS_PER_STRIP, PixelPin);

// holds chunksize x 3(chans per led) + 1 "action" byte
#define UDP_PACKET_SIZE ((CHUNK_SIZE*3)+1)
byte packetBuffer[ UDP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
RgbColor ledDataBuffer[ PIXELS_PER_STRIP];
byte r;
byte g;
byte b;

byte action;

// used later for holding values - used to dynamically limit brightness by amperage.
RgbColor prevColor;
int milliAmpsLimit = AMPS * 1000;
int milliAmpsCounter = 0;
byte millisMultiplier = 0;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//WiFiServer instance to query the module for status or to send commands to change some module settings //MDB
ESP8266WebServer server(80);

// Reply buffer, for now hardcoded but this might encompass useful data like dropped packets etc.
byte ReplyBuffer[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
byte counterHolder = 0;

//last number of frames to monitor monitoring //MDB
int const framesToMonitor = 600; //monitor last 10 seconds

struct framesMetaData {
          long frame;
         // int parts;
          long arrivedAt;
          int power; 
          int adjustedPower; 
          int processingTime;} framesMD[framesToMonitor];

static int frameNumber=0;
static int frameLimit=6000;
static int frameIndex= 0;
static int oldestFrameIndex=0;

long blankTime;
long processingTime;
long arrivedAt;

void setup() {

  ////////////////// A whole bunch of initialization stuff that prints no matter what.
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  WiFi.config(local_ip, gateway, subnet);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(UDP_PORT);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.print("Expected packagesize:");
  Serial.println(UDP_PACKET_SIZE);

  Serial.println("Setup done");
  Serial.println("");
  Serial.println("");
  Serial.println("");

  // Initial full black strip push and init.
  blankTime=micros();
  strip.Begin();
  for (uint16_t i = 0; i < PIXELS_PER_STRIP; i++) {
    strip.SetPixelColor(i, RgbColor(0, 0, 0));
    ledDataBuffer[i] = RgbColor(0, 0, 0);
  }
  strip.Show();
  blankTime=micros()-blankTime;

  // here we place all the different web services definition
  server.on("/getstatus", HTTP_GET, []() {
    server.send(200, "text/plain", "Connected to:"+String(ssid)+"\r\nFrame blanking time:"+String(blankTime)+"us\r\nFrames:"+String(frameNumber));
  });

  server.on("/getframes", HTTP_GET, []() {
    String r="Frame   Arrived At Power   X   CPU utilization\r\n";
    r+=      "    #         [us]  [ma]      [us]\r\n";
    //r+=String(oldestFrameIndex)+"-"+String(frameIndex)+"-"+String(framesToMonitor)+" - "+String(frameNumber)+"\r\n";
    for(int i=0 ; i<framesToMonitor ; i++) {
      if(framesMD[i].frame!=0) 
        r+=formatN(framesMD[i].frame,5)+" "+formatN(framesMD[i].arrivedAt,12)+" "+formatN(framesMD[i].power,5)+" "+formatN(framesMD[i].adjustedPower,3)+" "+formatN(framesMD[i].processingTime,5)+"\r\n";
    }
    server.send(200, "text/plain", r);
  });

  // Start the server //MDB
  server.begin();

}


void loop() {

  // if there's data available, read a packet
  int packetSize = udp.parsePacket();
  if (packetSize > 0)
  {
    //take initial time //MDB
    arrivedAt=micros();
    frameIndex=frameNumber % framesToMonitor ;
        
    // read the packet into packetBufffer
    udp.read(packetBuffer, UDP_PACKET_SIZE);

    action = packetBuffer[0];
    
    if (action==1) framesMD[frameIndex].arrivedAt=micros();
    
    if (DEBUG_MODE) { // If Debug mode is on print some stuff
      Serial.println("---Incoming---");
      Serial.print("Packet Size: ");
      Serial.println(packetSize);
      Serial.print("Action: ");
      Serial.println(action);
    }

    
    if (action != 0)
    { // if action byte is anything but 0 (this means we're receiving some portion of our rgb pixel data..)

      // Figure out what our starting offset is.
      const uint16_t initialOffset = CHUNK_SIZE * (action - 1);
      
      if (DEBUG_MODE) { // If Debug mode is on print some stuff
        Serial.print("---------: ");
        Serial.print(CHUNK_SIZE);
        Serial.print("   ");
        Serial.println((action - 1));
        Serial.println("");
        Serial.print("Init_offset: ");
        Serial.println(initialOffset);
        Serial.print(" ifLessThan: ");
        Serial.println((initialOffset + CHUNK_SIZE));
      }

      // loop through our recently received packet, and assign the corresponding
      // RGB values to their respective places in the strip.
      for (uint16_t i = 0; i < CHUNK_SIZE; i++) {

        r = packetBuffer[i * 3 + 1];
        g = packetBuffer[i * 3 + 2];
        b = packetBuffer[i * 3 + 3];

        strip.SetPixelColor(i + initialOffset, RgbColor(r, g, b));

        milliAmpsCounter += (r + g + b); // increment our milliamps counter accordingly for use later.
      }

      if (DEBUG_MODE) { // If Debug mode is on print some stuff
        Serial.println("Finished For Loop!");
      }

      // if we're debugging packet drops, modify reply buffer.
      if (PACKETDROP_DEBUG_MODE) {
        ReplyBuffer[action] = 1;
      }

      if (packetSize != UDP_PACKET_SIZE)
      { // if our packet was not full, it means it was also a terminating update packet.
        action = 0;
      }
    }

    // If we received an action byte of 0... this is telling us we received all the data we need
    // for this frame and we should update the actual rgb vals to the strip!
    framesMD[frameIndex].frame=frameNumber+1;
    framesMD[frameIndex].power=milliAmpsCounter;
    
    if (action == 0)
    {

      // this math gets our sum total of r/g/b vals down to milliamps (~60mA per pixel)
      milliAmpsCounter /= 13;
      framesMD[frameIndex].power=milliAmpsCounter;

      // because the Darken function uses a value from 0-255 this next line maths it into the right range and type.
      millisMultiplier = 255 - (byte)( constrain( ((float)milliAmpsLimit / (float)milliAmpsCounter), 0, 1 ) * 256);

      // Collect data  MDB
      framesMD[frameIndex].adjustedPower=millisMultiplier;

      if (DEBUG_MODE) { // If Debug mode is on print some stuff
        Serial.println("Trying to update leds...");
        Serial.print("Dimmin leds to: ");
        Serial.println( millisMultiplier );
      }


      // We already applied our r/g/b values to the strip, but we haven't updated it yet.
      // Sicne we needed the sum total of r/g/b values to calculate brightness, we
      // can loop through all the values again now that we have the right numbers
      // and scale brightness if we need to.
      for (int i = 0; i < PIXELS_PER_STRIP; i++) {
        prevColor = strip.GetPixelColor(i);
        prevColor.Darken(millisMultiplier);
        strip.SetPixelColor(i, prevColor);
      }

      strip.Show();   // write all the pixels out
      milliAmpsCounter = 0; // reset the milliAmpsCounter for the next frame.

      Serial.println("");  ////////////////////

      if (DEBUG_MODE) { // If Debug mode is on print some stuff
        Serial.println("Finished updating Leds!");
      }

      // Send reply to sender, basically a ping that says hey we just updated leds.
      //Serial.print("IP: ");
      //Serial.println(udp.remoteIP());
      //Serial.print("Port: ");
      //Serial.println(udp.remotePort());

      // if we're debugging packet drops, modify reply buffer.
      if (PACKETDROP_DEBUG_MODE) {
        // set the last byte of the reply buffer to 2, indiciating that the frame was sent to leds.
        ReplyBuffer[sizeof(ReplyBuffer) - 1] = 2;
        ReplyBuffer[0] = counterHolder;
        counterHolder += 1;
        // write out the response packet back to sender!
//        udp.beginPacket(udp.remoteIP(), UDP_PORT_OUT);
        // clear the response buffer string.
        for (byte i = 0; i < sizeof(ReplyBuffer); i++) {
//          udp.write(ReplyBuffer[i]);
          ReplyBuffer[i] = 0;
        }
//        udp.endPacket();
      }

    }

    

    if (DEBUG_MODE) { // If Debug mode is on print some stuff
      Serial.println("--end of packet and stuff--");
      Serial.println("");
    }

    //measure total frame processing time
    framesMD[frameIndex].processingTime=micros()-framesMD[frameIndex].arrivedAt;
    if(frameIndex=oldestFrameIndex) oldestFrameIndex=(oldestFrameIndex+1) % framesToMonitor;
    frameNumber=(frameNumber+1) % frameLimit;
    
  }
  //delay(0);
  server.handleClient(); //MDB give opportunity to process HTTP requests
}

String formatN(long n, int p) {
  String ns="       "+String(n);
  return ns.substring(ns.length()-p);
}

