// include some libraries
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NeoPixelBus.h>

// I have not tried more than 512 succesfully at 60 fps
// but I get glitching and stuttering and not sure where the bottleneck is exactly.
// at 30 fps I can go past this number succesfully though.
#define PIXELS_PER_STRIP 512
// This needs to be evenly divisible by PIXLES_PER_STRIP. 
// This represents how large our packets are that we send from our software source.
#define CHUNK_SIZE 128
// Dynamically limit brightness in terms of amperage.
#define AMPS 12

// Define some network variables here.
#define UDP_PORT 2390
IPAddress local_ip(192, 168, 1, 90);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
char ssid[] = "SSID_Name";  //  your network SSID (name)
char pass[] = "Password";       // your network password

// If this is set to 1, a lot of debug data will print to the console. 
// Will cause horrible stuttering meant for single frame by frame tests and such.
#define DEBUG_MODE 0


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

// used later for holding values - used to dynamically limit brightness by amperage.
RgbColor prevColor;
int milliAmpsLimit = AMPS * 1000;
int milliAmpsCounter = 0;
byte millisMultiplier = 0;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

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
  strip.Begin();
  for (uint16_t i = 0; i < PIXELS_PER_STRIP; i++) {
    strip.SetPixelColor(i, RgbColor(0, 0, 0));
    ledDataBuffer[i] = RgbColor(0, 0, 0);
  }
  strip.Show();
}


void loop() {
  
  // if there's data available, read a packet
  int packetSize = udp.parsePacket();
  if (packetSize > 0)
  {
    // read the packet into packetBufffer
    udp.read(packetBuffer, UDP_PACKET_SIZE);

    // get the action byte
    const byte action = packetBuffer[0];

    
    if(DEBUG_MODE){ // If Debug mode is on print some stuff
      Serial.println("---Incoming---");
      Serial.print("Packet Size: ");
      Serial.println(packetSize);
      Serial.print("Action: ");
      Serial.println(action);
    }

    
    // if action byte is anything but 0 (this means we're receiving some portion of our rgb pixel data..)
    if(action != 0)
    {
      // Figure out what our starting offset is.
      const uint16_t initialOffset = CHUNK_SIZE * (action-1);
      /*
      remainder = 0

      if(PIXELS_PER_STRIP-initialOffset < CHUNK_SIZE) 
      {
        remainder = PIXELS_PER_STRIP-initialOffset;
      }
      else
      {
        remainder = CHUNK_SIZE;
      }
      Serial.println(remainder);
      */

      if(DEBUG_MODE){ // If Debug mode is on print some stuff
        Serial.print("---------: ");
        Serial.print(CHUNK_SIZE);
        Serial.print("   ");
        Serial.println((action-1));
        Serial.println("");
        Serial.print("Init_offset: ");
        Serial.println(initialOffset);
        Serial.print(" ifLessThan: ");
        Serial.println((initialOffset+CHUNK_SIZE));
      }

      // loop through our recently received packet, and assign the corresponding
      // RGB values to their respective places in the strip.
      for (uint16_t i = 0; i < CHUNK_SIZE; i++) {

        r = packetBuffer[i*3+1];
        g = packetBuffer[i*3+2];
        b = packetBuffer[i*3+3];
         
         strip.SetPixelColor(i+initialOffset, RgbColor(r, g, b));

         milliAmpsCounter += (r+g+b); // increment our milliamps counter accordingly for use later.
      }

      if(DEBUG_MODE){ // If Debug mode is on print some stuff
        Serial.println("Finished For Loop!");
      }
      
    }

    // If we received an action byte of 0... this is telling us we received all the data we need
    // for this frame and we should update the actual rgb vals to the strip!
    else
    {

      // this math gets our sum total of r/g/b vals down to milliamps (~60mA per pixel)
      milliAmpsCounter /= 13; 

      // because the Darken function uses a value from 0-255 this next line maths it into the right range and type.
      millisMultiplier = 255-(byte)( constrain( ((float)milliAmpsLimit/(float)milliAmpsCounter),0,1 ) *256);
      
      if(DEBUG_MODE){ // If Debug mode is on print some stuff
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
        strip.SetPixelColor(i,prevColor);
      }
      strip.Show();   // write all the pixels out
      milliAmpsCounter = 0; // reset the milliAmpsCounter for the next frame.

      if(DEBUG_MODE){ // If Debug mode is on print some stuff
        Serial.println("Finished updating Leds!");
      }
    }

  if(DEBUG_MODE){ // If Debug mode is on print some stuff
    Serial.println("--end of packet and stuff--");
    Serial.println("");
  }
  }
}
