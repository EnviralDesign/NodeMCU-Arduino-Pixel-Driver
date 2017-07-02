// include some libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h> //MDB
#include <WiFiUdp.h>

//#include <NeoPixelBus.h>
#include <NeoPixelBrightnessBus.h> // instead of NeoPixelBus.h

// I have not tried more than 512 succesfully at 60 fps
// but I get glitching and stuttering and not sure where the bottleneck is exactly.
// at 30 fps I can go past this number succesfully though.
#define PIXELS_PER_STRIP 512

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
#define CHUNK_SIZE 171
#define MAX_ACTION_BYTE 4  //maximum numbers of chunks per frame in order to validate we do not receive a wrong index when there are communciation errors

// Dynamically limit brightness in terms of amperage.
#define AMPS 4


#define UDP_PORT 2390
#define UDP_PORT_OUT 2391
#define STREAMING_TIMEOUT 10  //  blank streaming frame after X seconds



// NETWORK_HOME
IPAddress local_ip(10, 10, 10, 200);
IPAddress gateway(10, 10, 10, 254); //LM
//IPAddress local_ip(192,168,1,200); //MDB
//IPAddress gateway(192, 168, 1, 1); //MDB
IPAddress subnet(255, 255, 255, 0);

char ssid[] = "ssid";   // your SSID
char pass[] = "pwd";       // your network password 


// If this is set to 1, a lot of debug data will print to the console.
// Will cause horrible stuttering meant for single frame by frame tests and such.
#define DEBUG_MODE 0 //MDB
#define PACKETDROP_DEBUG_MODE 0


//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 2;
//NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELS_PER_STRIP, PixelPin);
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELS_PER_STRIP, PixelPin);
NeoGamma<NeoGammaTableMethod> colorGamma;

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
int const framesToMonitor = 120; //monitor last 2 seconds
int frameNumber=0;
int frameLimit=6000;
int frameIndex= 0;
int oldestFrameIndex=0;
byte part=0;

struct framesMetaData {
          unsigned int frame;
          byte part;
          long arrivedAt;
          int packetSize;
          int power; 
          int adjustedPower; 
          long processingTime;} framesMD[framesToMonitor];



long blankTime;
long processingTime;
long arrivedAt;
long lastStreamingFrame=0;

volatile boolean streaming=true;

// variables to keep status during effects play
String play="";
String command="";
RgbColor rgb1=0;
RgbColor rgb2=0;
int times=1;
int frames=1;
int offset=0;
volatile int frame=0;

String rt;


void setup() {

  ////////////////// A whole bunch of initialization stuff that prints no matter what.
  Serial.begin(115200);
  Serial.println();
  Serial.println();


  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);  // WIFi STATION mode only
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

  for(int i=0 ; i<framesToMonitor ; i++)  //blank all frames metadata
     framesMD[i].frame=0;

  // here we place all the different web services definition
  server.on("/survey", HTTP_GET, []() {
    // build Javascript code to draw SVG wifi graph
    rt="<!doctype html><html><body>";
    rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="<br>port:"+String(UDP_PORT)+"<br>Expected packet size:"+String(UDP_PACKET_SIZE);
    rt+="<br><h2>WiFi monitoring</h2><svg id='svg' width='800' height='800'></svg><script type='text/javascript'>";
    rt+="var svgns = 'http://www.w3.org/2000/svg';var svg = document.getElementById('svg');";
    rt+="var color=0,colors = ['red','orange','blue','green','purple','cyan','magenta','yellow'];";
    rt+="line(80,400,640,400,'','');for(i=-1;i<=13;i++) {label='Ch'+i;if (i<1 || i>11 ) label='';line(i*40+120,400,i*40+120,80,null,label);}";
    rt+="for(i=-90;i<=-20;i+=10) {label=''+i+'dBm';line(80,-i*4,640,-i*4,label,null);}";
    rt+=getSSIDs();
    rt+="function line(x1,y1,x2,y2,lleft,ldown) {var l=document.createElementNS(svgns,'line');l.setAttribute('stroke','black');";
    rt+="l.setAttribute('x1', x1);l.setAttribute('y1', y1);l.setAttribute('x2', x2);l.setAttribute('y2', y2);svg.appendChild(l);";
    rt+="var t=document.createElementNS(svgns,'text');t.textContent = ldown;t.setAttribute('x', x1-15);t.setAttribute('y', y1+20);svg.appendChild(t);";
    rt+="var t=document.createElementNS(svgns,'text');t.textContent = lleft;t.setAttribute('x', x1-60);t.setAttribute('y', y1+5); svg.appendChild(t);}";
    rt+="function channel(ch,db,ssid) {var p=document.createElementNS(svgns,'path');";
    rt+="p.setAttribute('d', 'M'+((ch-2)*40+120)+' 400 l60 '+(-400-db*4)+' l40 0 L'+((ch+2)*40+120)+' 400');";
    rt+="p.setAttribute('stroke',colors[color]);p.setAttribute('stroke-width',3);p.setAttribute('fill','none');svg.appendChild(p);";
    rt+="var t=document.createElementNS(svgns,'text');t.setAttribute('stroke',colors[color]);t.textContent = ssid;";
    rt+="t.setAttribute('x', ch*40-(ssid.length/2*8)+120);t.setAttribute('y', -db*4-5);svg.appendChild(t); color=(color+1) % colors.length;}";  
    rt+="</script></body></html>";
    server.send(200, "text/html", rt);
  });

  server.on("/getstatus", HTTP_GET, []() {
    // build Javascript code to draw SVG wifi graph
    rt="<!doctype html><html><body>";
    rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="<br>port:"+String(UDP_PORT)+"<br>Expected packet size:"+String(UDP_PACKET_SIZE);
    rt+="</body></html>";
    server.send(200, "text/html", rt);
  });

  server.on("/getframes", HTTP_GET, []() {
    rt="<html><body><table>";
    rt+="<tr style=\"border:1px solid black\"><th>Frame<br>#</th><th>Action<br>byte</td><th>Arrived At<br>[µS]</th><th>Packet Size<br>[bytes]</th><th>Power<br>[mA]</th><th>Power<br>Adjustment</th><th>Packet CPU<br>time [µS]</th><th>Frame CPU<br>time [µS]</th></tr>";
    long acum=0;
    long lastFrame=0;
    boolean grey=false;
    for(int i=0 ; i<framesToMonitor ; i++) {
       if(framesMD[i].frame!=0) {
        if (framesMD[i].frame!=lastFrame) {
          grey=!grey;
          lastFrame=framesMD[i].frame;
        };
        if(grey)
          rt += "<tr align=\"center\" bgcolor=\"lightgrey\">";
        else 
          rt += "<tr align=\"center\">";
        rt += "<td>"+String(framesMD[i].frame)+"</td>";
        rt += "<td>"+String(framesMD[i].part)+"</td>";
        rt += "<td>"+String(framesMD[i].arrivedAt)+"</td>"; 
        rt += "<td>"+StringZ(framesMD[i].packetSize)+"</td>";
        rt += "<td>"+StringZ(framesMD[i].power)+"</td>";
        rt += "<td>"+StringZ(framesMD[i].adjustedPower)+"</td>";
        rt += "<td>"+StringZ(framesMD[i].processingTime)+"</td>";
        if(framesMD[i].part==1) acum = framesMD[i].processingTime;
        if(framesMD[i].part>1)  acum = acum + framesMD[i].processingTime;
        if(framesMD[i].part==0)  {
          rt+="<td>"+String(acum+framesMD[i].processingTime)+"</td>";
          acum=0;}
         else
          rt+="<td></td>";
         rt+="</tr>";
       };
    };
    rt+="</table></body></html>";
    server.send(200, "text/html; charset=UTF-8", rt);
  });


  server.on("/play", HTTP_POST, []() {
    // <effect> [RGB[r1],[g1],[b1]] [RGB[r],[g2],[b2]] [T<times>] [F<frames>]
    // Executes "effect" with the specified parameters 
    // Ej: blink rgb255,0,0 rgb0,0,0 t10 f10
    // Blinks red / black for 10 times showing each color during 10 frames.
    play=server.arg("plain");  //retrieve body from HTTP POST request
    streaming=false; //quit streaming mode and enter effects playing mode into main loop
    frame=0; //means it needs to fetch command from play sequence
    offset=0; // start parsing new line from leftmost character
    //Serial.println("POST request");    
    server.send(200,"text/plain", "OK");
    });

  // Start the server //MDB
  server.begin();

}

void blankFrame() {
  paintFrame(RgbColor(0,0,0));
}

void paintFrame(RgbColor c) {
  for (uint16_t i = 0; i < PIXELS_PER_STRIP; i++) strip.SetPixelColor(i, c);
  strip.Show();
}

String getCommand(String line){
  String ret=line.substring(0,line.indexOf(' '));
  ret.toUpperCase();
  return ret;
};

String getParams(String line) {
  if(line.indexOf(' ')==-1) 
    return "";
  else {
    String ret=line.substring(line.indexOf(' ')+1);
    ret.toUpperCase();
    return ret;
  }; 
};

RgbColor getRGB(String params) {
  return getRGB(params,1);
}

RgbColor getRGB(String params, int n) {
  byte r=0;g=0;b=0;
  int pos=0;
  String colors;
  for (int i=1;i<=n;i++) {
    pos=params.indexOf('RGB',pos)+1;
  }
  if(pos>0) { //found RGB token at pos
    colors=params.substring(pos,(params.substring(pos)+" ").indexOf(' ')+pos);//extract colors  values between RGB token and next space/LF
    Serial.println(params+" -> "+String(n)+":"+String(pos)+" <"+colors+">");
    r=constrain(colors.toInt(),0,255); // isolate red component
    pos=colors.indexOf(',');
    if (pos>=0) {
      g=constrain(colors.substring(pos+1).toInt(),0,255); // isolate green component
      pos=colors.indexOf(',',pos+1);
      if (pos>=0) 
        b=constrain(colors.substring(pos+1).toInt(),0,255); //isolate blue component
    }
  }
  return RgbColor(r,g,b);
};

int getFrames(String params) {
  int f=1;
  int pos=params.indexOf("F");
  if(pos>=0) 
    f=abs(params.substring(pos+1).toInt());
  return f;
}

int getTimes(String params) {
  int t=1;
  int pos=params.indexOf("T");
  if(pos>=0)
    t=abs(params.substring(pos+1).toInt());
  return t;
}


void loop() {
  if(streaming) {
    playStreaming();
  } else {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) udp.read();  //remove any udp packet from buffer while in effect mode
    if(!playEffect()) { //when last frame of last effect switch back to streaming mode
      streaming=true;
    };
    delay(16);
  };
  server.handleClient();
}

boolean playEffect() {
  if(frame==0) {  // frame zero means to go get a command line from the HTTP request body
    String line,params;
    int pos;
    if(offset>=play.length()) { // when "play sequence" is finished had to go back to streaming mode
      return false;
    } else { // try to fetch next "play sequence" line
      line="";
      while (offset<play.length() && byte(play[offset])!=10) {  // extract line
        line+=play[offset++];
      };
      if(byte(play[offset])==10) offset++; // skip line feed

      command=getCommand(line);  //Parse all parameters 
      params=getParams(line);
      rgb1=getRGB(params,1);
      rgb2=getRGB(params,2);
      times=getTimes(params);
      frames=getFrames(params);
      
      frame++;        // advance to frame 1 to start animating effect
   };   
 };

 //place here pointers to all Effect functions
 if(command=="BLINK") blink(rgb1, rgb2, frames, times);

 return true;
}

void blink(RgbColor rgb1, RgbColor rgb2, int frames, int times) {
  // Blink all leds showing each color during "frames" frames and for "times" times

  if(frame >= frames*2*times) { //if already played all frames & times, it means the effect ended
    frame=0; 
  } else {
    if(frame % (frames*2) <= frames)   //calculate which frame is within each time, if it is within the first half show rgb1 otherwise rgb2
      paintFrame(rgb1);
    else
      paintFrame(rgb2);
    frame++;
  };
  return;
}

void playStreaming() {
  // if there's data available, read a packet
 // digitalWrite(BUILTIN_LED,LOW);
 if(lastStreamingFrame!=0 && millis()-lastStreamingFrame>STREAMING_TIMEOUT*1000) {
  blankFrame();
  lastStreamingFrame=0;
 }
  int packetSize = udp.parsePacket();
  if (packetSize > 0)
  {
    //take initial time //MDB
    arrivedAt=micros();
   
    // read the packet into packetBufffer
    udp.read(packetBuffer, UDP_PACKET_SIZE);

    action = packetBuffer[0];
   
    //Serial.println(String(frameIndex)+":"+frameNumber+" - "+String(action)+" "+String(packetSize)+"/"+String(UDP_PACKET_SIZE)); 
    
    if (DEBUG_MODE) { // If Debug mode is on print some stuff
      Serial.println("---Incoming---");
      Serial.print("Packet Size: ");
      Serial.println(packetSize);
      Serial.print("Action: ");
      Serial.println(action);
    }
    
    if (action != 0)
    { // if action byte is anything but 0 (this means we're receiving some portion of our rgb pixel data..)

      framesMD[frameIndex].frame=frameNumber;
      framesMD[frameIndex].part=action;
      framesMD[frameIndex].arrivedAt=arrivedAt;
      
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

      if(action<=MAX_ACTION_BYTE) { //check the ation byte is within limits
        uint16_t led=0;
        for (uint16_t i = 1; i < CHUNK_SIZE*3;) {

          r = packetBuffer[i++];
          g = packetBuffer[i++];
          b = packetBuffer[i++];

          //strip.SetPixelColor(i + initialOffset, RgbColor(r, g, b)); // this line does not use gamma correction
          strip.SetPixelColor(initialOffset+led++, colorGamma.Correct(RgbColor(r, g, b))); // this line uses gamma correction

          milliAmpsCounter += (r + g + b); // increment our milliamps counter accordingly for use later.
        }
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
      framesMD[frameIndex].packetSize=packetSize;
      framesMD[frameIndex].power=0;
      framesMD[frameIndex].adjustedPower=0;
      framesMD[frameIndex].processingTime=micros()-framesMD[frameIndex].arrivedAt;
      frameIndex=(frameIndex +1) % framesToMonitor;
      packetSize=0; // implicit frame 0 will have packetSize == 0  MDBx
      arrivedAt=micros();
    }

    // If we received an action byte of 0... this is telling us we received all the data we need
    // for this frame and we should update the actual rgb vals to the strip!
    //framesMD[frameIndex].frame=frameNumber+1;  //MDBx
    
    if (action == 0)
    {
      framesMD[frameIndex].frame=frameNumber;
      framesMD[frameIndex].part=action;
      framesMD[frameIndex].arrivedAt=arrivedAt;

      pinMode(BUILTIN_LED, OUTPUT);

      // this math gets our sum total of r/g/b vals down to milliamps (~60mA per pixel)
      milliAmpsCounter /= 13;
      framesMD[frameIndex].power=milliAmpsCounter;

      // because the Darken function uses a value from 0-255 this next line maths it into the right range and type.
      millisMultiplier = 255 - (byte)( constrain( ((float)milliAmpsLimit / (float)milliAmpsCounter), 0, 1 ) * 256);
      millisMultiplier = map(millisMultiplier, 0, 255, 255, 0); // inverse the multiplier to work with new brightness control method
      // Collect data  MDB
      framesMD[frameIndex].adjustedPower=millisMultiplier;

      if (DEBUG_MODE) { // If Debug mode is on print some stuff
        Serial.println("Trying to update leds...");
        Serial.print("Dimmin leds to: ");
        Serial.println( millisMultiplier );
      }


      // We already applied our r/g/b values to the strip, but we haven't updated it yet.
      // Since we needed the sum total of r/g/b values to calculate brightness, we
      // can loop through all the values again now that we have the right numbers
      // and scale brightness if we need to.
      
      //for (int i = 0; i < PIXELS_PER_STRIP; i++) {
      //  prevColor = strip.GetPixelColor(i);
      //  prevColor.Darken(millisMultiplier);
      //  strip.SetPixelColor(i, prevColor);
      //}

      if(millisMultiplier!=255)  //dim LEDs only if required
        strip.SetBrightness(millisMultiplier); // this new brightness control method was added to lib recently, affects entire strip at once.
      strip.Show();   // write all the pixels out
      lastStreamingFrame=millis();
      milliAmpsCounter = 0; // reset the milliAmpsCounter for the next frame.

      //Serial.println("");  ////////////////////

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
        udp.beginPacket(udp.remoteIP(), UDP_PORT_OUT);
        // clear the response buffer string.
        for (byte i = 0; i < sizeof(ReplyBuffer); i++) {
          udp.write(ReplyBuffer[i]);
          ReplyBuffer[i] = 0;
        }
        udp.endPacket();
      }

      //measure total frame processing time
      framesMD[frameIndex].processingTime=micros()-framesMD[frameIndex].arrivedAt;
      frameIndex=(frameIndex+1) % framesToMonitor;

    
      //framesMD[frameIndex].processingTime=micros()-framesMD[frameIndex].arrivedAt;
      if(frameIndex==oldestFrameIndex) oldestFrameIndex=(oldestFrameIndex+1) % framesToMonitor;
      frameNumber=(frameNumber+1) % frameLimit;

      pinMode(BUILTIN_LED, INPUT);
    }

    if (DEBUG_MODE) { // If Debug mode is on print some stuff
      Serial.println("--end of packet and stuff--");
      Serial.println("");
    }
    
  }
  //delay(0);

}

String formatN(long n, int p) {
  String ns="       "+String(n);
  return ns.substring(ns.length()-p);
}

String StringZ(int n) {
  if(n==0)
    return "";
  else
    return String(n);
}

String getSSIDs() { // build wifi information channel calls for JS code
  String s="";
  byte n;
  n=WiFi.scanNetworks(false,true);
  for(int i=0;i<n;i++)      s+="channel("+String(WiFi.channel(i))+","+String(WiFi.RSSI(i))+",'"+WiFi.SSID(i)+"');";
  return s;
};


