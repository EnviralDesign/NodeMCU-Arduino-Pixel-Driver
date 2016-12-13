#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NeoPixelBus.h>

#define PIXELS_PER_STRIP 512
#define CHUNK_SIZE 128
#define AMPS 12

#define UDP_PORT 2390
IPAddress local_ip(192, 168, 1, 90);

IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);

#define DEBUG_MODE 0


//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 2;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELS_PER_STRIP, PixelPin);

char ssid[] = "CampoGrande";  //  your network SSID (name)
char pass[] = "8177937134";       // your network password

#define UDP_PACKET_SIZE ((CHUNK_SIZE*3)+1)
byte packetBuffer[ UDP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
RgbColor ledDataBuffer[ PIXELS_PER_STRIP];
RgbColor prevColor;
int milliAmpsLimit = AMPS * 1000;
int milliAmpsCounter = 0;

byte millisMultiplier = 0;

byte r;
byte g;
byte b;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

void setup() {
  
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
    
    const byte action = packetBuffer[0];

    ///*
    if(DEBUG_MODE){
      Serial.println("---Incoming---");
      Serial.print("Packet Size: ");
      Serial.println(packetSize);
      Serial.print("Action: ");
      Serial.println(action);
    }
    //*/

    

    if(action != 0)
    {
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

      ///*
      if(DEBUG_MODE){

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
      //*/
      
      for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
         //ledDataBuffer[i+initialOffset] = RgbColor(packetBuffer[i*3+1], packetBuffer[i*3+2], packetBuffer[i*3+3]);

        r = packetBuffer[i*3+1];
        g = packetBuffer[i*3+2];
        b = packetBuffer[i*3+3];
         
         strip.SetPixelColor(i+initialOffset, RgbColor(r, g, b));
         milliAmpsCounter += (r+g+b);
      }

      ///*
      if(DEBUG_MODE){
        Serial.println("Finished For Loop!");
      }
      //*/
      
    }

    else
    {

      milliAmpsCounter /= 13;
      millisMultiplier = 255-(byte)( constrain( ((float)milliAmpsLimit/(float)milliAmpsCounter),0,1 ) *256);
      
      ///*
      if(DEBUG_MODE){
        Serial.println("Trying to update leds...");
        Serial.print("Dimmin leds to: ");
        Serial.println( millisMultiplier );
      }
      //*/

      //Serial.println(millisMultiplier);
      for (int i = 0; i < PIXELS_PER_STRIP; i++) {
        prevColor = strip.GetPixelColor(i);
        prevColor.Darken(millisMultiplier);
        strip.SetPixelColor(i,prevColor);
      }
      strip.Show();   // write all the pixels out
      milliAmpsCounter = 0;

      ///*
      if(DEBUG_MODE){
        Serial.println("Finished updating Leds!");
      }
      //*/
    }

  ///*
  if(DEBUG_MODE){
    Serial.println("--end of packet and stuff--");
    Serial.println("");
  }
  //*/
  }
}
