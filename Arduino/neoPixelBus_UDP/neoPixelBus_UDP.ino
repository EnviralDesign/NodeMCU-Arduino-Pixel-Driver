// include some libraries
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>
#include <WiFiManager.h>
#include <DoubleResetDetector.h>
#include "FS.h"ftp
#include <ESP8266FtpServer.h>

#include <EnviralDesign.h>

#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS); 

///////////////////// USER DEFINED VARIABLES START HERE /////////////////////////////
// NOTICE: these startup settings, especially pertaining to number of pixels and starting color
// will ensure that your nodeMCU can be powered on and run off of a usb 2.0 port of your computer.

String deviceName = "PxlNode-8266";

// number of physical pixels in the strip.
uint16_t pixelsPerStrip = 256;

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
uint16_t chunkSize = 64;

// Dynamically limit brightness in terms of amperage.
float amps = 3;
uint16_t mAPerPixel = 60;

// UDP port to receive streaming data on.
uint16_t udpPort = 2390;

//Set here the inital RGB color to show on module power up
byte InitColor[] = {200, 75, 10};

///////////////////// USER DEFINED VARIABLES END HERE /////////////////////////////

//maximum numbers of chunks per frame in order to validate we do not receive a wrong index when there are communciation errors
#define MAX_ACTION_BYTE 4

//Interfaces user defined variables with memory stored in EEPROM
EnviralDesign ed(&pixelsPerStrip, &chunkSize, &mAPerPixel, &deviceName, &amps, &udpPort, InitColor);

//FTP server to store bitmaps
FtpServer ftpSrv;

#define UDP_PORT_OUT 2391
#define STREAMING_TIMEOUT 10  //  blank streaming frame after X seconds

// NETWORK_HOME
//IPAddress local_ip(10, 10, 10, 200);
//IPAddress gateway(10, 10, 10, 254); //LM
//IPAddress local_ip(192,168,1,200); //MDB
//IPAddress gateway(192, 168, 1, 1); //MDB
//IPAddress subnet(255, 255, 255, 0);

//char ssid[] = "SSID";   // your SSID
//char pass[] = "PWD";       // your network password 


// If this is set to 1, a lot of debug data will print to the console.
// Will cause horrible stuttering meant for single frame by frame tests and such.
#define DEBUG_MODE 0 //MDB
#define PACKETDROP_DEBUG_MODE 0


//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 2;
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> *strip;
NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelAnimator animations(1); //Number of animations

NeoVerticalSpriteSheet<NeoBufferMethod<NeoGrbFeature>> *spriteSheet;

//Variables used by Sprite Object
uint16_t spritePixels = 16;
uint16_t spriteFrames = 20;
uint32_t spriteCounter = 0;
uint32_t spriteRepeat = 1;
uint32_t spriteAnimationTime = 60;
uint32_t fileAddressPixels = 0;
uint32_t sizeRow = 0;
uint8_t bytesPerPixel = 3;
bool bottomToTop;
uint8_t *imageBuffer;
uint16_t indexSprite;
bool playingSprite = false;

// holds chunksize x 3(chans per led) + 1 "action" byte
uint16_t udpPacketSize;
byte * packetBuffer;
RgbColor * ledDataBuffer;
byte r;
byte g;
byte b;

//Set here the inital RGB color to show on module power up
RgbColor InitialColor;
RgbColor LastColor=RgbColor(0,0,0);  //hold the last colour in order to stitch one effect with the following.


byte action;

// used later for holding values - used to dynamically limit brightness by amperage.
RgbColor prevColor;
int milliAmpsLimit = amps * 1000;
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

// Default Settings
String settings="mAPerPixel 60 Amps 4 name Enviral topology RA 10 10"; //mAPerPixel and Amps no longer used here

String topology;

// variables to keep status during effects play
String play="";
String command="";
RgbColor rgb1=RgbColor(0,0,0);
RgbColor rgb2=RgbColor(0,0,0);
HsbColor hsb1=HsbColor(0,0,0);
HsbColor hsb2=HsbColor(0,0,0);
HslColor hsl1=HslColor(0,0,0);
HslColor hsl2=HslColor(0,0,0);

int times=1;
int frames=1;
int offset=0;
volatile int frame=0;

WiFiManager wifiManager;

void setup() {
  if (drd.detectDoubleReset()) { //if user double clicks reset button, then reset wifisetting
    wifiManager.resetSettings();
    drd.stop();
  }
 
  ////////////////// A whole bunch of initialization stuff that prints no matter what.
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println(F("Serial started")); 

  ed.setCompile(String(__TIME__));    //Compiling erases variables previously changed over the network
  ed.start();
 
  setUdpPacketSize();
  
  //Initializes NeoPixelBus
  startNeoPixelBus();

  //Initialize SpriteObject
  spriteSetup();
  
  // We start by connecting to a WiFi network
  //Serial.print("Connecting to ");
  //Serial.println(ssid);
 // WiFi.mode(WIFI_STA);  // WIFi STATION mode only
  //WiFi.begin(ssid, pass);
  //WiFi.config(local_ip, gateway, subnet);

  //Animate from dark to initial color in 3 seconds on module power up
  initDisplay();

  //while (WiFi.status() != WL_CONNECTED) {
  //  delay(500);
  //  Serial.print(".");
  //}
  //Serial.println("");

  wifiManager.autoConnect("Enviral");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //SPIFFS Setup  FTP Server
  if (!SPIFFS.begin()) {
    Serial.println(F("Failed to mount file system"));
    while(1) {
      delay(1000);
    }
  }
  ftpSrv.begin("esp8266", "esp8266");  //Username and password

  startUDP();
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.print("Expected packagesize:");
  Serial.println(udpPacketSize);

  Serial.println("Setup done");
  Serial.println("");
  Serial.println("");
  Serial.println("");

  // Initial full black strip push and init.
  blankTime=micros();
  //strip->Begin();
  //for (uint16_t i = 0; i < pixelsPerStrip; i++) {
  //  strip->SetPixelColor(i, RgbColor(0, 0, 0));
  //  ledDataBuffer[i] = RgbColor(0, 0, 0);
  //}
  //strip->Show();
  blankTime=micros()-blankTime;

  for(int i=0 ; i<framesToMonitor ; i++)  //blank all frames metadata
     framesMD[i].frame=0;

  // here we place all the different web services definition

  server.on("/survey", HTTP_GET, []() {
    // build Javascript code to draw SVG wifi graph
    IPAddress local_ip=WiFi.localIP();
    String rt="<!doctype html><html><body>";
//    rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="Connected to:"+WiFi.SSID()+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="<br>port:"+String(udpPort)+"<br>Expected packet size:"+String(udpPacketSize);
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
    IPAddress local_ip=WiFi.localIP();
    String rt="<!doctype html><html><body>";
    //rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="Connected to:"+WiFi.SSID()+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="<br>port:"+String(udpPort)+"<br>Expected packet size:"+String(udpPacketSize);
    rt+="</body></html>";
    server.send(200, "text/html", rt);
  });
  
  
  server.on("/mcu_info", HTTP_GET, []() {
    // build javascript-like data
    IPAddress local_ip=WiFi.localIP();
    FSInfo fs_info;

    String rt = "name:"+deviceName;
    rt += ",";
    rt += "ip:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt += ",";
    rt += "ssid:"+WiFi.SSID();
    rt += ",";
    rt += "port:"+String(udpPort);
    rt += ",";
    rt += "packetsize:"+String(udpPacketSize);
    server.send(200, "text/html", rt);
  });

  server.on("/mcu_json", HTTP_GET, []() {
    // build json data
    IPAddress local_ip=WiFi.localIP();
    String rt;
    FSInfo fs_info;
    StaticJsonBuffer<1000> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["device_name"] = deviceName;
    root["ip"] = String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    root["ssid"] = String(WiFi.SSID());
    root["udp_streaming_port"] = udpPort;
    root["packetsize"] = udpPacketSize;
    root["chunk_size"] = chunkSize;
    root["pixels_per_strip"] = pixelsPerStrip;
    root["ma_per_pixel"] = mAPerPixel;
    root["amps_limit"] = amps;
    JsonArray& wcArr = root.createNestedArray("warmup_color");
    wcArr.add(InitColor[0]);
    wcArr.add(InitColor[1]);
    wcArr.add(InitColor[2]);
    if (SPIFFS.info(fs_info)) {
      
      root["totalBytes"] = fs_info.totalBytes;
      root["usedBytes"] = fs_info.usedBytes;
      root["blockSize"] = fs_info.blockSize;
      root["pageSize"] = fs_info.pageSize;
      root["maxOpenFiles"] = fs_info.maxOpenFiles;
      root["maxPathLength"] = fs_info.maxPathLength;
    }
    root.printTo(rt);
    server.send(200, "application/json", rt);
  });

  server.on("/getframes", HTTP_GET, []() {
    String rt="<html><body><table>";
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
    playingSprite = false;  //Stop playing the sprite animation
    frame=0; //means it needs to fetch command from play sequence
    offset=0; // start parsing new line from leftmost character
    //Serial.println("POST request");
    server.send(200,"text/plain", "OK");
    });

  server.on("/mcu_config", HTTP_POST, []() {
    if (server.hasArg("plain") == false) {
      server.send(422, "application/json", "{\"error\":\"HTTP BODY MISSING\"}");
      return;
    }
    String updateString = server.arg("plain");  //retrieve body from HTTP POST request
    Serial.println(updateString);
    drd.stop(); //Prevents WiFi wiping during resets
    StaticJsonBuffer<2000> jsonBuffer;
    JsonObject& input = jsonBuffer.parseObject(server.arg("plain"));
    if (!input.success()) {     //Not a well formed json. Checking for regular command
      if (updateString.length() > 64) {
        server.send(422, "application/json", "{\"error\":\"COMMAND TOO LONG\"}");
        return;
      }
      char str[64];
      updateString.toCharArray(str, 64);
      String cmd = strtok(str, " ");
      if (cmd.indexOf("pixels_per_strip") == 0) {
        Serial.println("Updating");
        blank();
        int val = String(strtok(NULL, " ")).toInt();
        
        if (!updatePixels(val)) {
          server.send(422, "application/json", "{\"error\":\"PARAMETER OUT OF RANGE\",\"pixels_per_strip\":\"Failed\"}");
          return;
        }
        server.send(200,"application/json", "{\"pixels_per_strip\":\"Success\"}");
        startNeoPixelBus();
        initDisplay();
        //restart();  //restart no longer needed for pixel update
        
      } else if (cmd.indexOf("chunk_size") == 0) {
        int val = String(strtok(NULL, " ")).toInt();
        if (!updateChunk(val)) return;
        setUdpPacketSize();
        server.send(200,"application/json", "{\"chunk_size\":\"Success\"}");
        
      } else if (cmd.indexOf("ma_per_pixel") == 0) {
        int val = String(strtok(NULL, " ")).toInt();
        if (!updateMA(val)) return;
        initDisplay();
        server.send(200,"application/json", "{\"ma_per_pixel\":\"Success\"}");
        
      } else if (cmd.indexOf("device_name") == 0) {
        if (!updateName(String(strtok(NULL, " ")))) return;
        server.send(200,"application/json", "{\"device_name\":\"Success\"}");
        
      } else if (cmd.indexOf("amps_limit") == 0) {
        float val = String(strtok(NULL, " ")).toFloat();
        if (!updateAmps(val)) return;
        milliAmpsLimit = amps * 1000;
        initDisplay();
        server.send(200,"application/json", "{\"amps_limit\":\"Success\"}");
        
      } else if (cmd.indexOf("udp_streaming_port") == 0) {
        int val = String(strtok(NULL, " ")).toInt();
        if (!updateUDP(val)) return;
        startUDP();
        server.send(200,"application/json", "{\"udp_streaming_port\":\"Success\"}");
        //blankRestart();
        
      } else if (cmd.indexOf("warmup_color") == 0) {
        byte v1 = String(strtok(NULL, " ")).toInt();
        byte v2 = String(strtok(NULL, " ")).toInt();
        byte v3 = String(strtok(NULL, " ")).toInt();
        updateWarmUp(v1, v2, v3);
        initDisplay();
        server.send(200,"application/json", "{\"warmup_color\":\"Success\"}");
        
      } else {
        Serial.println(cmd);
        server.send(422,"application/json", "{\"error\":\"INVALID COMMAND\"}");
      } 
      
    } else {  //JSON detected
      JsonObject& root = jsonBuffer.createObject();
      String rt;
      blank();
      bool initD = false;

      if (input["pixels_per_strip"] != NULL) {
        root["pixels_per_strip"] = (updatePixels(input["pixels_per_strip"]) ? "Success" : "Failed");
        startNeoPixelBus();
        initD = true;
      }
      
      if (input["chunk_size"] != NULL) {
        root["chunk_size"] = (updateChunk(input["chunk_size"]) ? "Success" : "Failed");
        setUdpPacketSize();
      }
      
      if (input["ma_per_pixel"] != NULL) {
        root["ma_per_pixel"] = (updateMA(input["ma_per_pixel"]) ? "Success" : "Failed");
        initD = true;
      }
      
      if (input["device_name"].success()) {
        const char* dName = input["device_name"];
        Serial.println(dName);        
        root["device_name"] = (updateName(String(dName)) ? "Success" : "Failed");
      }
      
      if (input["amps_limit"] != NULL) {
        root["amps_limit"] = (updateAmps(input["amps_limit"]) ? "Success" : "Failed");
        milliAmpsLimit = amps * 1000;
        initD = true;
      }
      
      if (input["udp_streaming_port"] != NULL) {
        root["udp_streaming_port"] = (updateUDP(input["udp_streaming_port"]) ? "Success" : "Failed");
        startUDP();
      }
      
      if (input["warmup_color"] != NULL ) {
        //byte arr[3] = input["warmup_color"];
        //char arr[16];
        //temp.toCharArray(arr, 16);
        byte v1 = input["warmup_color"][0];
        byte v2 = input["warmup_color"][1];
        byte v3 = input["warmup_color"][2];
        root["warmup_color"] = (updateWarmUp(v1, v2, v3) ? "Success" : "Failed");
        initD = true;
      }
      root.printTo(rt);
      server.send(200, "application/json", rt);
      if (initD) initDisplay();
    }
    
   });
  
  // Start the server //MDB
  server.begin();
}

void blankFrame() {
  paintFrame(RgbColor(0,0,0));
};

void paintFrame(RgbColor c) {
  //c=adjustToMaxMilliAmps(c); // do not allow to exceed max current
  for (uint16_t i = 0; i < pixelsPerStrip; i++) strip->SetPixelColor(i, c);
  strip->Show();
};

RgbColor adjustToMaxMilliAmps(RgbColor c) {
  float ma= (mAPerPixel/3) * (c.R+c.G+c.B) /255.0 * pixelsPerStrip;//float ma=20*(c.R+c.G+c.B)/255.0*pixelsPerStrip;
  RgbColor r=c;
  if (ma > milliAmpsLimit)  {// need to adjust down
    r.R=c.R*milliAmpsLimit/ma;
    r.G=c.G*milliAmpsLimit/ma;
    r.B=c.B*milliAmpsLimit/ma;
  }
   //Serial.println("milliAmpsLimit:"+String(milliAmpsLimit)+" ma:"+String(ma));
   //Serial.println("adjustToMaxMilliAmps :"+String(c.R)+" "+String(c.G)+" "+String(c.B)+" -> "+String(r.R)+" "+String(r.G)+" "+String(r.B));
  return r;
}; 

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
    // Serial.println(params+" -> "+String(n)+":"+String(pos)+" <"+colors+">");
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

int getColors(String params,String colorSystem) {
  int pos=0;
  int c=0;
  while ((pos=params.indexOf(colorSystem,pos))>=0) {
    c++;
    pos++;
  };
  return c;
}

RgbColor getHSB(String params) {
  return getHSB(params,1);
}

RgbColor getHSB(String params, int n) {
  int h=0,s=0,b=0;
  int pos=0;
  String colors;
  for (int i=1;i<=n;i++) {
    pos=params.indexOf('HSB',pos)+1;
  }
  if(pos>0) { //found HSB token at pos
    colors=params.substring(pos,(params.substring(pos)+" ").indexOf(' ')+pos);//extract colors  values between HSB token and next space/LF
    // Serial.println(params+" -> "+String(n)+":"+String(pos)+" <"+colors+">");
    h=constrain(colors.toInt(),0,360); // isolate Hue component
    pos=colors.indexOf(',');
    if (pos>=0) {
      s=constrain(colors.substring(pos+1).toInt(),0,100); // isolate Saturation component
      pos=colors.indexOf(',',pos+1);
      if (pos>=0) 
        b=constrain(colors.substring(pos+1).toInt(),0,100); //isolate Brightness component
    }
  }
  RgbColor r=RgbColor(255,0,0);
  HsbColor hsb=HsbColor(r);
  //Serial.println("Color: R:"+String(r.R)+" G:"+String(r.G)+" B:"+String(r.B)+" / h:"+String(hsb.H)+" s:"+String(hsb.S)+" b:"+String(hsb.B));; 
  //Serial.println("h:"+String(h)+" s:"+String(s)+" b:"+String(b));

  return HsbColor(h/360.0f,s/100.0f,b/100.0f);
};

HslColor getHSL(String params) {
  return getHSL(params,1);
}

HslColor getHSL(String params, int n) {
  int h=0,s=0,l=0;
  int pos=0;
  String colors;
  for (int i=1;i<=n;i++) {
    pos=params.indexOf('HSL',pos)+1;
  }
  if(pos>0) { //found HSL token at pos
    colors=params.substring(pos,(params.substring(pos)+" ").indexOf(' ')+pos);//extract colors  values between HSL token and next space/LF
    // Serial.println(params+" -> "+String(n)+":"+String(pos)+" <"+colors+">");
    h=constrain(colors.toInt(),0,360); // isolate Hue component
    pos=colors.indexOf(',');
    if (pos>=0) {
      s=constrain(colors.substring(pos+1).toInt(),0,100); // isolate Saturation component
      pos=colors.indexOf(',',pos+1);
      if (pos>=0) 
        l=constrain(colors.substring(pos+1).toInt(),0,100); //isolate Lightness component
    }
  }
  
  return HslColor(h/360.0f,s/100.0f,l/100.0f);
};

void getSettings(String params) {
  int pos=0;int pos2=0;
  String topo="RA";
  int x=1;int y=1;
  params.toUpperCase();
  params+=" "; //add space terminator
  
  // get milliamps per logical pixel
  pos=params.indexOf("MAPERPIXEL");
  if(pos>0) mAPerPixel=params.substring(pos+1).toInt();
  
  //get total Amps budget
  pos=params.indexOf("AMPS");
  if(pos>0) amps=params.substring(pos+1).toInt();

  //get module name
  pos=params.indexOf("NAME");
  if(pos>=0) {
    while (params.substring(pos,pos+1)==" ") pos++; //skip spaces
    pos2=params.indexOf(" ",pos);
    if (pos2>=0) deviceName=params.substring(pos,pos2);
  };
  
  //get topology
  pos=params.indexOf("TOPOLOGY");
  while (params.substring(pos,pos+1)==" ") pos++; //skip spaces
  if(params.substring(pos,pos+2)=="R ") topo="R";
  if(params.substring(pos,pos+2)=="C ") topo="C";
  if(params.substring(pos,pos+3)=="RA ") topo="RA";
  if(params.substring(pos,pos+3)=="CA ") topo="CA";
  pos=params.indexOf(" ",pos);
  while (params.substring(pos,pos+1)==" ") pos++; //skip spaces
  x=params.substring(pos).toInt();
  pos=params.indexOf(" ",pos+1);
  y=params.substring(pos).toInt();

  return;
}



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


void loop() { //main program loop
  if(streaming) {
    playStreaming();
  } else if (playingSprite){
    if (spriteCounter > spriteRepeat) {
      blank();
    } else {
      animations.UpdateAnimations();
      strip->Show();
    }
  } else {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) udp.read();  //remove any udp packet from buffer while in effect mode
    if(!playEffect()) { //when last frame of last effect switch back to streaming mode
      streaming=true;
    };
    delay(16);
  };
  ftpSrv.handleFTP();
  server.handleClient();
}

boolean playEffect() {
  Serial.println(play);
  if(frame==0) {  // frame zero means to go get a command line from the HTTP request body
    String line,params;
    int pos,RGBcolors,HSBcolors,HSLcolors;
    if(offset>=play.length()) { // when "play sequence" is finished had to go back to streaming mode
      return false;
    } else { // try to fetch next "play sequence" line
      line="";
      while (offset<play.length() && byte(play[offset])!=10) {  // extract line
        line+=play[offset++];
      };
      if(byte(play[offset])==10) offset++; // skip line feed

      command=getCommand(line);  //Parse all parameters
      if (command.equals("SPRITE")) return handleSprite();
      params=getParams(line);
      // Get RBG colors
      RGBcolors=getColors(params,"RGB");
      //Serial.println("Colors:"+String(colors));
      if(RGBcolors==1) {
        rgb1=LastColor;
        rgb2=adjustToMaxMilliAmps(getRGB(params,1));
      } else {
        rgb1=adjustToMaxMilliAmps(getRGB(params,1));  //retrieve RGB parameter and adjust down to stay within power limit
        rgb2=adjustToMaxMilliAmps(getRGB(params,2));   //retrieve RGB parameter and adjust down to stay within power limit
      };
      if(RGBcolors>0) LastColor=rgb2;

      // Get HSB colors
      HSBcolors=getColors(params,"HSB");
      if(HSBcolors==1) {
        hsb1=LastColor;
        hsb2=getHSB(params,1);
      } else {
        hsb1=getHSB(params,1);
        hsb2=getHSB(params,2);
      };
      if(HSBcolors>0) LastColor=hsb2;

      // Get HSL colors
      HSLcolors=getColors(params,"HSL");
      if(HSLcolors==1) {
        hsl1=LastColor;
        hsl2=getHSL(params,1);
      } else {
        hsl1=getHSL(params,1);
        hsl2=getHSL(params,2);
      };
      if(HSLcolors>0) LastColor=hsl2;     

      times=getTimes(params);
      frames=getFrames(params);
      
      //frame++;        // advance to frame 1 to start animating effect
   };   
 };
 Serial.println("command:" + command + " rgb1:" + rgb1.R+","+rgb1.G+","+rgb1.B + " rgb2:" + rgb2.R+","+rgb2.G+","+rgb2.B + " duration(frames):" + frames + " repetitions:" + times);
 
 //place here pointers to all Effect functions
 if(command=="BLINK") blink(rgb1, rgb2, frames, times);
 if(command=="HUE")   hue(rgb1, rgb2, frames, times);
 if(command=="HUE2")  hue2(rgb1, rgb2, frames, times); 
 if(command=="PULSE") pulse(rgb1, rgb2, frames, times);
 if(command=="BLANK") blankFrame();
 if(command=="HUEHSB") huehsb(hsb1,hsb2,frames, times);
 if(command=="HUEHSL") huehsl(hsl1,hsl2,frames, times);

 return true;
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
    udp.read(packetBuffer, udpPacketSize);

    action = packetBuffer[0];
   
    //Serial.println(String(frameIndex)+":"+frameNumber+" - "+String(action)+" "+String(packetSize)+"/"+String(udpPacketSize)); 
    
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
      const uint16_t initialOffset = chunkSize * (action - 1);
      
      if (DEBUG_MODE) { // If Debug mode is on print some stuff
        Serial.print("---------: ");
        Serial.print(chunkSize);
        Serial.print("   ");
        Serial.println((action - 1));
        Serial.println("");
        Serial.print("Init_offset: ");
        Serial.println(initialOffset);
        Serial.print(" ifLessThan: ");
        Serial.println((initialOffset + chunkSize));
      }

      // loop through our recently received packet, and assign the corresponding
      // RGB values to their respective places in the strip.
      if(action<=MAX_ACTION_BYTE) { //check the ation byte is within limits
        uint16_t led=0;
        for (uint16_t i = 1; i < chunkSize*3;) {

          r = packetBuffer[i++];
          g = packetBuffer[i++];
          b = packetBuffer[i++];

          strip->SetPixelColor(initialOffset+led++, colorGamma.Correct(RgbColor(r, g, b))); // this line uses gamma correction
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

      if (packetSize != udpPacketSize)
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
      
      if(millisMultiplier!=255)  //dim LEDs only if required
        strip->SetBrightness(millisMultiplier); // this new brightness control method was added to lib recently, affects entire strip at once.
      strip->Show();   // write all the pixels out
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

String sHSB(HsbColor h) {
  return "(H:"+String(h.H)+",S:"+String(h.S)+",B:"+String(h.B)+")";
}

String sHSL(HslColor h) {
  return "(H:"+String(h.H)+",S:"+String(h.S)+",L:"+String(h.L)+")";
}

String getMac() {
  byte mac[6];
  WiFi.macAddress(mac);
  return String(mac[0],HEX)+String(mac[1],HEX)+String(mac[2],HEX)+String(mac[3],HEX)+String(mac[4],HEX)+String(mac[5],HEX)+String(mac[6],HEX);
}

// *** PLACE EFFECT FUNCTIONS BELOW ****

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


void hue(RgbColor rgb1, RgbColor rgb2, int frames, int times) {
  // linear transition from rgb1 color to rgb2 color in "frames" frames, repeating "times" times

  if(frame >= frames*times) { //if already played all frames & times, it means the effect ended
    frame=0; 
  } else {
    //int f=frame % frames;
    float f=(frame % frames)*frames/(frames-1.0);  
    //transition from rgb1 to rgb2
    paintFrame(RgbColor(rgb1.R+(rgb2.R-rgb1.R)*f/frames,rgb1.G+(rgb2.G-rgb1.G)*f/frames,rgb1.B+(rgb2.B-rgb1.B)*f/frames));
    frame++;
  };
  return;
}

void hue2(RgbColor rgb1, RgbColor rgb2, int frames, int times) {
  // linear transition from rgb1 color to rgb2 color in "frames" frames and back to rgb1, repeating "times" times

  if(frame >= frames*2*times) { //if already played all frames & times, it means the effect ended
    frame=0; 
  } else {
    int f=frame % (frames*2);
    
    if(f <= frames)   //calculate if it is transitioning from rgb1 to rgb2 or the opposite
      //transition from rgb1 to rgb2
      paintFrame(RgbColor(rgb1.R+(rgb2.R-rgb1.R)*f/frames,rgb1.G+(rgb2.G-rgb1.G)*f/frames,rgb1.B+(rgb2.B-rgb1.B)*f/frames));
    else
      paintFrame(RgbColor(rgb1.R+(rgb2.R-rgb1.R)*(2*frames-f)/frames,rgb1.G+(rgb2.G-rgb1.G)*(2*frames-f)/frames,rgb1.B+(rgb2.B-rgb1.B)*(2*frames-f)/frames));
    frame++;
  };
  return;
}

void pulse(RgbColor rgb1, RgbColor rgb2, int frames, int times) {
  // linear transition from rgb1 color to rgb2 color in "frames" frames, then blank frame and wait for "frames" frames, repeating "times" times

  if(frame >= frames*2*times) { //if already played all frames & times, it means the effect ended
    frame=0; 
  } else {
    int f=frame % (frames*2);
    
    if(f <= frames)   //calculate if it is transitioning from rgb1 to rgb2 or the opposite
      //transition from rgb1 to rgb2
      paintFrame(RgbColor(rgb1.R+(rgb2.R-rgb1.R)*f/frames,rgb1.G+(rgb2.G-rgb1.G)*f/frames,rgb1.B+(rgb2.B-rgb1.B)*f/frames));
    else
      blankFrame();
    frame++;
  };
  return;
}

void huehsb(HsbColor hsb1, HsbColor hsb2, int frames, int times) {
  // linear transition between hsb1 ro hsb2 color around the HSB wheel during "frames" frames and repeated "times" times
  HsbColor hsb;
  if(frame >= frames*times) { //if already played all frames & times, it means the effect ended
    frame=0; 
  } else {
    float progress=(frame % frames)/(frames-1.0);  
    //transition from rgb1 to rgb2
    hsb=HsbColor(hsb1.H+(hsb2.H-hsb1.H)*progress,hsb1.S+(hsb2.S-hsb1.S)*progress,hsb1.B+(hsb2.B-hsb1.B)*progress);
    //Serial.print(progress);Serial.println(sHSB(hsb1)+" - "+sHSB(hsb2)+":"+sHSB(hsb));
    paintFrame(RgbColor(hsb));
    frame++;
  };
  return;
}

void huehsl(HslColor hsl1, HslColor hsl2, int frames, int times) {
  // linear transition between hsb1 ro hsb2 color around the HSL wheel during "frames" frames and repeated "times" times
  // Hue 0-360 gives the color, Saturation 0-100, Lightness: 0=black, 50= solid color, 100=white
  HslColor hsl;
  if(frame >= frames*times) { //if already played all frames & times, it means the effect ended
    frame=0;  
  } else {
    float progress=(frame % frames)/(frames-1.0);  
    //transition from rgb1 to rgb2
    hsl=HslColor(hsl1.H+(hsl2.H-hsl1.H)*progress,hsl1.S+(hsl2.S-hsl1.S)*progress,hsl1.L+(hsl2.L-hsl1.L)*progress);
    //Serial.print(progress);Serial.println(sHSL(hsl1)+" - "+sHSL(hsl2)+":"+sHSL(hsl));
    paintFrame(RgbColor(hsl));
    frame++;
  };
  return;
}

bool updatePixels(int val) {
  if (val > 1500) {
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

void blank() {
  play="blank";
  streaming=false;
  playingSprite=false;
  frame=0;
  offset=0;
  playEffect();
}

void restart() {  
  Serial.println(F("Restarting..."));
  delay(50);
  WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while(1)wdt_reset();
}

void startNeoPixelBus() {
  Serial.println(F("Starting NeoPixelBus"));

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

void setUdpPacketSize() {
  if (packetBuffer) free(packetBuffer);  
  udpPacketSize = ((chunkSize*3)+1);
  packetBuffer = (byte *)malloc(udpPacketSize);//buffer to hold incoming and outgoing packets
}

void startUDP() {
  Serial.println(F("Starting UDP"));
  udp.stop();
  delay(100);
  udp.begin(udpPort);
}

void initDisplay() {  
  milliAmpsCounter = 0;
  millisMultiplier = 0;
  InitialColor=RgbColor(InitColor[0],InitColor[1],InitColor[2]); 
  InitialColor=adjustToMaxMilliAmps(InitialColor);
  for(int i=0;i<=90;i++) {
    paintFrame(RgbColor(InitialColor.R*i/90.0,InitialColor.G*i/90.0,InitialColor.B*i/90.0));
    delay(16);
  };
}

void LoopAnimUpdate(const AnimationParam& param) {
  // wait for this animation to complete,
  // we are using it as a timer of sorts
  if (param.state == AnimationState_Completed)
  {
      // done, time to restart this position tracking animation/timer
      animations.RestartAnimation(param.index);

      // draw the next frame in the sprite
      spriteSheet->Blt(*strip, 0, indexSprite);
      indexSprite = (indexSprite + 1) % spriteFrames; // increment and wrap
      if (indexSprite == 0) spriteCounter++;
  }  
}

//bool readBitmapInfo(File _file) {  //Extracts metadata from BitMap File
//  BitmapFileHeader bmpHeader;
//  BitmapInfoHeader bmpInfoHeader;
//  size_t result;
//
//  result = _file.read((uint8_t*)(&bmpHeader), sizeof(bmpHeader));
//
//  if (result != sizeof(bmpHeader) ||
//      bmpHeader.FileId != c_BitmapFileId ||
//      bmpHeader.FileSize != _file.size()) {
//        goto error;
//  }
//
//  result = _file.read((uint8_t*)(&bmpInfoHeader), sizeof(bmpInfoHeader));
//
//  if (result != sizeof(bmpInfoHeader) ||
//      result != bmpInfoHeader.Size ||
//      1 != bmpInfoHeader.Planes ||
//      BI_Rgb != bmpInfoHeader.Compression) {
//        goto error;
//  }
//
//  if(!(24 == bmpInfoHeader.BitsPerPixel ||    //RGB
//       32 == bmpInfoHeader.BitsPerPixel)) {  //RGBW
//        goto error;
//  }
//
//  imageWidth = abs(bmpInfoHeader.Width);
//  imageHeight = abs(bmpInfoHeader.Height);
//  fileAddressPixels = bmpHeader.PixelAddress;
//  bottomToTop = (bmpInfoHeader.Height > 0);
//  sizeRow = (bmpInfoHeader.BitsPerPixel * imageWidth + 31) / 32 * 4;
//  bytesPerPixel = bmpInfoHeader.BitsPerPixel / 8;
//  
//  return true;
//  error:
//    fileAddressPixels = 0;
//    imageWidth = 0;
//    imageHeight = 0;
//    sizeRow = 0;
//    bytesPerPixel = 0;
//    return false;
//}

//bool convertBitmap(File f, uint16_t xIn, uint16_t yIn) { //Transfers bytes from bitmap to memory
//  free(imageBuffer);
//  uint16_t xFile = xIn, yFile = yIn;
//  
//  imageBuffer = (uint8_t *)malloc(bytesPerPixel * xIn * yIn);
//  int bufferCount = 0;
//  for (uint16_t y = 0; y < imageHeight; y++) {
//    if (xIn >= imageWidth) xFile = imageWidth - 1;
//    if (yIn + y >= imageHeight) yFile = imageHeight -1 + y;
//    if (seek(f, xFile, yFile)) {
//      for (int16_t x = 0; x < xIn; x++) {
//        if (xFile < imageWidth) {
//          uint8_t bgr[4];
//          if (f.read(bgr, bytesPerPixel) != bytesPerPixel) {
//            imageBuffer[bufferCount++] = 0;
//            imageBuffer[bufferCount++] = 0;
//            imageBuffer[bufferCount++] = 0;
//            if (bytesPerPixel == 4) imageBuffer[bufferCount++] = 0;  //Write zeros
//          } else {
//            imageBuffer[bufferCount++] = bgr[0];
//            imageBuffer[bufferCount++] = bgr[1];
//            imageBuffer[bufferCount++] = bgr[2];
//            if (bytesPerPixel == 4) imageBuffer[bufferCount++] = bgr[3];
//            xFile++;
//          }          
//        }
//      }
//    }
//  }  
//}

//bool loadSpriteFileBak(File f, uint16_t xIn, uint16_t yIn, uint16_t frames, byte cStart[], byte cEnd[]) {
//  free(imageBuffer);
//  imageBuffer = (uint8_t *)malloc(bytesPerPixel * xIn * yIn * frames);
//  int bufferCount = 0;
//  double frameDiff[bytesPerPixel];
//  for (byte fI = 0; fI < bytesPerPixel; fI++) {
//    frameDiff[fI] = double(cEnd[fI] - cStart[fI]) / double(frames);
//  }
//  for (uint16_t frameCounter = 0; frameCounter < frames; frameCounter++) {
//    uint16_t xFile = xIn, yFile = yIn;
//    byte cAdjust[bytesPerPixel];
//    for (byte cI = 0; cI < bytesPerPixel; cI++) {
//      //                    15  *   255/30
//      cAdjust[cI] = round(double(frameCounter + 1) * frameDiff[cI]);
//    }
//    for (uint16_t y = 0; y < imageHeight; y++) {
//      if (xIn >= imageWidth) xFile = imageWidth - 1;
//      if (yIn + y >= imageHeight) yFile = imageHeight -1 + y;
//      if (seek(f, xFile, yFile)) {
//        for (int16_t x = 0; x < xIn; x++) {
//          if (xFile < imageWidth) {
//            uint8_t bgr[4];
//            if (f.read(bgr, bytesPerPixel) != bytesPerPixel) {
//              imageBuffer[bufferCount++] = 0;
//              imageBuffer[bufferCount++] = 0;
//              imageBuffer[bufferCount++] = 0;
//              if (bytesPerPixel == 4) imageBuffer[bufferCount++] = 0;  //Write zeros
//            } else {
//              for (byte i = 0; i < bytesPerPixel; i++) {                
//                imageBuffer[bufferCount++] = min(bgr[i] + cAdjust[i], 255);
//              }            
//  //            imageBuffer[bufferCount++] = min(bgr[0] + , 255);
//  //            imageBuffer[bufferCount++] = min(bgr[1] + , 255);
//  //            imageBuffer[bufferCount++] = min(bgr[2] + , 255);
//  //            if (bytesPerPixel == 4) imageBuffer[bufferCount++] = min(bgr[3] + , 255);
//              xFile++;
//            }
//          }
//        }
//      }
//    }
//  }
//  spriteSheet = new NeoVerticalSpriteSheet<NeoBufferMethod<NeoGrbFeature>>(imageWidth * imageHeight, frames, 1, imageBuffer);  
//  return true;
//}

// Frames = 10, cStart = [0,0,0] cEnd = [255,255,255] cAdjust = [0 + 255-0/10 * f]
// BiLinear Blend: 
//  Upper Left: cStart 
//  Lower Right: cEnd
//  Upper Right & Lower Left: value in file
bool loadSpriteFile(File f, uint16_t xIn, uint16_t yIn, uint16_t frames, uint8_t cStartIn[], uint8_t cEndIn[]) {
  free(imageBuffer);
  String imgParams = "X: " + String(xIn) + ", Y: " + String(yIn) + ", Frames: " + String(frames);
  imgParams += ", Start: (" + String(cStartIn[0]) + ", " + String(cStartIn[1]) + ", " + String(cStartIn[2]) + ")";
  imgParams += ", End: (" + String(cEndIn[0]) + ", " + String(cEndIn[1]) + ", " + String(cEndIn[2]) + ")";
  Serial.println(imgParams);
  imageBuffer = (uint8_t *)malloc(bytesPerPixel * xIn * yIn * frames);
  int bufferCount = 0;
  double frameDiff[bytesPerPixel];
  float progress = 0;
  RgbColor cStart = RgbColor(cStartIn[0], cStartIn[1], cStartIn[2]);
  RgbColor cEnd = RgbColor(cEndIn[0], cEndIn[1], cEndIn[2]);
  for (byte fI = 0; fI < bytesPerPixel; fI++) {
    frameDiff[fI] = double(cEndIn[fI] - cStartIn[fI]) / double(frames);
  }
  for (uint16_t frameCounter = 0; frameCounter < frames; frameCounter++) {
    byte cAdjust[bytesPerPixel];
    for (byte cI = 0; cI < bytesPerPixel; cI++) {
      //                    15  *   255/30
      progress = double(frameCounter + 1) / double(frames);
      //cAdjust[cI] = cStart[cI] + round(double(frameCounter + 1) * frameDiff[cI]);
    }
    for (uint16_t y = 0; y < yIn; y++) {
      for (uint16_t x = 0; x < xIn; x++) {          
        uint8_t bgr[bytesPerPixel];
        f.read(bgr, bytesPerPixel);
        RgbColor spriteColor = RgbColor(bgr[0], bgr[1], bgr[2]);
        RgbColor result = RgbColor::BilinearBlend(
          cStart,                 //Upper Left
          spriteColor,            //Upper Right
          spriteColor,            //Lower Left
          cEnd,                   //Lower Right
          progress,               //X axis
          0.5f                    //Y axis
        );
        imageBuffer[bufferCount++] = result.R;
        imageBuffer[bufferCount++] = result.G;
        imageBuffer[bufferCount++] = result.B;       
      }
    }
  }
  delete spriteSheet;
  spriteSheet = new NeoVerticalSpriteSheet<NeoBufferMethod<NeoGrbFeature>>(xIn * yIn, frames, 1, imageBuffer);  
  return true;
}

//bool seek(File f, uint16_t x, uint16_t y) { //Used from bitmap data extraction
//  if (bottomToTop) {
//    y = (spriteFrames - 1) - y;
//  }
//  uint32_t pos = y * sizeRow + x * bytesPerPixel;
//  pos += fileAddressPixels;
//  return f.seek(pos, SeekCur);
//}

bool spriteParse() {
  char buf[256];
  char *tok;
  Serial.print("Sprite to parse: ");
  Serial.println(play);
  //Make sure all arguments are present
  int rgbCount = 0;
  int sCount = 0;
  int tCount = 0;
  int fCount = 0;
  int xCount = 0;
  int yCount = 0;
  int aCount = 0;
  int i = 0;
  int commaCount = 0;
  play.toCharArray(buf, 256);

  tok = strtok(buf, " ");  //Assumes SPRITE

  while (tok = strtok(NULL, " ")) {
    char c = tok[0];
    switch(c) {
      case 'r':
        if (tok[1] != 'g' || tok[2] != 'b') return false;
        i = 3;
        commaCount = 0;
        while (tok[i] != '\0' && i < 14) {
          if (isDigit(tok[i])) {
            i++;
          } else if (tok[i] == ',') {
            commaCount++;
            i++;
          } else {
            return false;
          }          
        }
        if (commaCount != 2) return false;
        rgbCount++;
        break;
      case 's':
        if (tok[1] == '\'') {
          i = 2;
          while (tok[i] != '\'' && i < 63) {
            switch (tok[i]) {
              case '\0':
              case '\\':
              case '/':
              case ':':
              case '*':
              case '"':
              case '<':
              case '>':
              case '|':
              case '?':
                return false;
              default:
                i++;
            }
          }
          if (tok[i] != '\'' || i < 3) return false;
        } else if (!checkDigits(1, tok)) {
          return false;
        }
        sCount++;
        break;
      case 't':
        if (!checkDigits(1, tok)) return false;
        tCount++;
        break;
      case 'f':
        if (!checkDigits(1, tok)) return false;
        fCount++;
        break;
      case 'x':
        if (!checkDigits(1, tok)) return false;
        xCount++;
        break;
      case 'y':
        if (!checkDigits(1, tok)) return false;
        yCount++;
        break;
      case 'a':
        if (!checkDigits(1, tok)) return false;
        aCount++;
        break;
      default:
        return false;
    }
  }

  if (rgbCount == 2 && sCount == 1 && tCount == 1 && fCount == 1 && xCount == 1 && yCount == 1 && aCount == 1) return true;
  else return false;
}

bool checkDigits(int index, char* arr) {
  while (arr[index] != '\0' && index < 63) {
    if (!isDigit(arr[index])) return false;
    index++;
  }
  return true;
}

int getDigits(int index, char* arr) {
  char temp[12];
  int i = 0;
  while (arr[i+index] != '\0' && i < 11) {
    temp[i] = arr[i+index];
    i++;
  }
  temp[i] = '\0';
  return atoi(temp);
}

bool handleSprite() { //SPRITE rgb255,0,0 rgb0,0,255 t10 f30 s1 x4 y3 a60
                      //SPRITE rgb255,0,0 rgb0,0,255 t10 f30 s'filename.bmp' x8 y32 a60
  if (!spriteParse()) {
    Serial.println("Sprite failed to parse");
    return false;
  }
  char buf[256];
  char *tok;

  bytesPerPixel = 3; //Set bytes per pixel to 3 as default
  //Incoming Parameters
  String filename = "/";
  uint8_t colorStart[3]  = {0, 0, 0};
  uint8_t colorEnd[3] = {0, 0, 0};
  char cTemp[4] = {'\0'};
  bool firstColor = true;
  uint32_t timeRepeats = 0;
  uint32_t numFrames = 0;
  int8_t fileNum = -1;
  uint16_t x = 0;
  uint16_t y = 0;
  int i = 0;
  int tI = 0;
  int cI = 0;
  Serial.print("Sprite to tokenize");Serial.println(play);
  play.toCharArray(buf, 256);
  tok = strtok(buf, " ");     //Discards command SPRITE
  Serial.print("First token: ");Serial.println(tok);
  while (tok = strtok(NULL, " ")) {
    Serial.print("Next token: ");Serial.println(tok);
    char c = tok[0];
    switch (c) {
      case 'r':
        char temp[12];
        i = 0;
        while (tok[3+i] != '\0' && i < 11) {
          temp[i] = tok[3+i];
          i++;
        }
        temp[i] = '\0';        
        i = 0;
        tI = 0;
        cI = 0;
        do {         
          if (temp[i] == ',' || temp[i] == '\0') {
            cTemp[tI] = '\0';
            if (firstColor){
              colorStart[cI] = atoi(cTemp);
              Serial.print("First RGB: ");Serial.println(String(colorStart[0]) + ":" + String(colorStart[1]) + ":" + String(colorStart[2]));
            } else {
              colorEnd[cI] = atoi(cTemp);
              Serial.print("Second RGB: ");Serial.println(String(colorEnd[0]) + ":" + String(colorEnd[1]) + ":" + String(colorEnd[2]));
            }
            if (temp[i] == '\0') {
              break;
            }
            tI = 0;
            cI++;
          } else {
            cTemp[tI] = temp[i];
            tI++;
          }
          i++;
        } while (i < 11);        
        firstColor = false;
        
        break;
      case 't':
        timeRepeats = getDigits(1, tok);
        break;
      case 'f':
        numFrames = getDigits(1, tok);
        break;
      case 'a':
        spriteAnimationTime = getDigits(1, tok);
        break;
      case 's':
        if (tok[1] == '\'') {
          i = 2;
          while (tok[i] != '\'') {
            filename += String(tok[i]);
            i++;
          }
        } else {
          fileNum = getDigits(1, tok);
        }
        break;
      case 'x':
        x = getDigits(1, tok);
        break;
      case 'y':
        y = getDigits(1, tok);
        break;
      default:
        return false;
    }
  }
  Serial.print("Last token: ");Serial.println(tok);

  if (fileNum > 0) {
    filename += "sp" + String(fileNum);
  }
  Serial.print("Filename: ");Serial.println(filename);
  File f = SPIFFS.open(filename, "r");
  if (!f) {
    Serial.println(F("file open failed"));
    return false;
  }
//    if (!readBitmapInfo(f)) { //If loading failed play what's in memory
//      Serial.println(F("Failed to get bitmap info"));
//      f.close();
//      playingSprite = true;
//      indexSprite = 0;
//      animations.StartAnimation(0, 60, LoopAnimUpdate);
//      return true;
//    }
  if (!loadSpriteFile(f, x, y, numFrames, colorStart, colorEnd)) {
    Serial.println(F("Failed to convert bitmap into memory"));
    return false;
  }
  
  f.close();
  blankFrame();
  playingSprite = true;
  indexSprite = 0;
  spriteCounter = 0;
  spriteFrames = numFrames;
  spriteRepeat = timeRepeats;
  animations.StartAnimation(0, spriteAnimationTime, LoopAnimUpdate);
  return true;
}

void spriteSetup() {  //Loads default sprite object
  free(imageBuffer);
  delete spriteSheet;
  spriteCounter = 0;
  imageBuffer = (uint8_t *)malloc(3 * 16 * 20);  //Bytes per Pixel * Pixels * Frames
  uint8_t tempBuffer[] = {  // (16 x 20) GRB in Hexadecimal
    //{<-------------->} ONE PIXEL    
       0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //ONE FRAME BLOCK
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 
       
       0x00, 0x3f, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x7f,    0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x3f, 0x00, 0x00, 0x7f, 0x00, 0x00, 
       0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x3f, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xff, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00,    0x00, 0x7f, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x7f,     
       0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x3f, 0x00, 0x00, 0x7f, 0x00, 0x00,    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x3f, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x3f, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x01, 0x00, 0x00, 0xff, 0x00, 0x00,    0x7e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xff,     
       0x00, 0x00, 0x7e, 0x00, 0x00, 0x3e, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,    0x00, 0xff, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x3e,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x01, 0x00, 0x00, 0xff, 0x00, 0x00, 0x7e, 0x00,    0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x01, 0x00, 0x00, 0xff, 0x00, 0x00, 
       0x7e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,    0x00, 0x00, 0x7f, 0x00, 0x00, 0x3e, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x3f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       
       0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
  };
  for (int i = 0; i < 3*16*20; i++) {
    imageBuffer[i] = tempBuffer[i];
  }
  indexSprite = 0;
  spriteCounter = 0;
  spriteFrames = 20;
  spriteRepeat = 100;
  spriteSheet = new NeoVerticalSpriteSheet<NeoBufferMethod<NeoGrbFeature>>(16, 20, 1, imageBuffer);  
}


