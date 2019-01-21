#include <ArduinoJson.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DoubleResetDetector.h>
//#include <ESP8266_WiFiManager.h>
#include <WiFiManager.h>
#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
//WiFiServer instance to query the module for status or to send commands to change some module settings //MDB
ESP8266WebServer server(80);

#else
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
//WiFiServer instance to query the module for status or to send commands to change some module settings //MDB
WebServer server(80);
#endif

#include <WiFiUdp.h>
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>
#include <DNSServer.h>
#include <FS.h>

#ifdef ESP32
#include <SPIFFS.h>
#define FORMAT_SPIFFS_IF_FAILED true
#endif

#include <EnviralDesign.h>

// UDP Poll Opcodes
#define CHUNKIDMIN 0
#define CHUNKIDMAX 99
#define UPDATEFRAME 100
#define POLL 200
#define POLLREPLY 201
#define CONFIG 202
#define NOPACKET -1
#define UDPID "EnviralDesignPxlNode"
#define IDLENGTH 20

int opcode;
bool minFrameTimeMet = true;

// Common strings to save memory
const char successStr[] PROGMEM = {"SUCCESS!"};
const char failStr[] PROGMEM = {"FAILURE!"};
const char pixStripJSON[] PROGMEM = {"pixels_per_strip"};
const char devNameJSON[] PROGMEM = {"device_name"};
const char udpPortJSON[] PROGMEM = {"udp_streaming_port"};
const char chunkSzJSON[] PROGMEM = {"chunk_size"};
const char maPixelJSON[] PROGMEM = {"ma_per_pixel"};
const char ampJSON[] PROGMEM = {"amps_limit"};
const char initColJSON[] PROGMEM = {"warmup_color"};
const char frameTimeJSON[] PROGMEM = {"frame_time"};
const char textHTML[] PROGMEM = {"text/html"};
const char textJSON[] PROGMEM = {"text/json"};
const char textPlain[] PROGMEM = {"text/plain"};
const char appJSON[] PROGMEM = {"application/json"};
const char plain[] PROGMEM = {"plain"};
const char ipJSON[] PROGMEM = {"ip"};
const char ssidJSON[] PROGMEM = {"ssid"};
const char packetSzJSON[] PROGMEM = {"packetsize"};
const char totalBytesJSON[] PROGMEM = {"totalBytes"};
const char usedBytesJSON[] PROGMEM = {"usedBytes"};
const char blockSizeJSON[] PROGMEM = {"blockSize"};
const char pageSizeJSON[] PROGMEM = {"pageSize"};
const char maxOpenFilesJSON[] PROGMEM = {"maxOpenFiles"};
const char maxPathLengthJSON[] PROGMEM = {"maxPathLength"};
//File server to transfer bitmaps
const char host[] PROGMEM = {"esp8266fs"};

//#define min(a,b) ((a)<(b)?(a):(b))
//#define max(a,b) ((a)>(b)?(a):(b))

///////////////////// USER DEFINED VARIABLES START HERE /////////////////////////////
// NOTICE: these startup settings, especially pertaining to number of pixels and starting color
// will ensure that your nodeMCU can be powered on and run off of a usb 2.0 port of your computer.

String deviceName = "PxlNode-8266";

// number of physical pixels in the strip.
uint16_t pixelsPerStrip = 640;

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
uint16_t chunkSize = 64;

// Dynamically limit brightness in terms of amperage.
float amps = 1;
uint16_t mAPerPixel = 60;

// UDP port to receive streaming data on.
uint16_t udpPort = 2390;

//Set here the inital RGB color to show on module power up
byte InitColor[] = {200, 75, 10};

#ifdef ESP8266
uint16_t udpMinFrameTime = 16; // 33 = 30 fps, 22 = 45 fps, 16 = 60fps, 8 = 120fps
#else
uint16_t udpMinFrameTime = 1;
#endif
///////////////////// USER DEFINED VARIABLES END HERE /////////////////////////////

//maximum numbers of chunks per frame in order to validate we do not receive a wrong index when there are communciation errors
#define MAX_ACTION_BYTE 4

//Interfaces user defined variables with memory stored in EEPROM
EnviralDesign ed(&pixelsPerStrip, &chunkSize, &mAPerPixel, &deviceName, &amps, &udpPort, InitColor);

//holds the current upload
File fsUploadFile;

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
bool DEBUG_MODE = false; //MDB
bool PACKETDROP_DEBUG_MODE = false;

//#define pixelPin D4  // make sure to set this to the correct pin, ignored for UartDriven branch
const uint8_t PixelPin = 21;
//#ifdef ESP8266
NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> *strip;
//#else
//NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> *strip;
//#endif
NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelAnimator animations(1); //Number of animations
NeoPixelAnimator effectAnimations(1);

NeoVerticalSpriteSheet<NeoBufferMethod<NeoGrbFeature>> *spriteSheet;

//Variables used by Sprite Object
#define MAX_SPRITESIZE 30000
uint16_t spritePixels = 16;
uint16_t spriteFrames = 20;
uint8_t bytesPerPixel = 3;
uint8_t *imageBuffer;
uint16_t indexSprite;
volatile bool playingSprite = false;
String spritelastfile = "";
uint8_t lastColorStart[3];
uint8_t lastColorEnd[3];

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


//byte action;

// used later for holding values - used to dynamically limit brightness by amperage.
RgbColor prevColor;
uint32_t milliAmpsLimit = amps * 1000;
uint32_t milliAmpsCounter = 0;
byte millisMultiplier = 0;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

// Reply buffer, for now hardcoded but this might encompass useful data like dropped packets etc.
byte ReplyBuffer[IDLENGTH + 1 + MAX_NAME_LENGTH + 16] = {0};
byte counterHolder = 0;

//last number of frames to monitor monitoring //MDB
int const framesToMonitor = 120; //monitor last 2 seconds

int frameLimit=6000;
int frameIndex= 0;

struct framesMetaData {
          unsigned int frame;
          int part;
          unsigned long arrivedAt;
          int packetSize;
          int power; 
          int adjustedPower; 
          unsigned long processingTime;} framesMD[framesToMonitor];

unsigned long arrivedAt;
unsigned long lastStreamingFrame=0;

volatile boolean streaming=true;

// variables to keep status during effects play
String play="";
String command="";
RgbColor rgb1=RgbColor(0,0,0);
RgbColor rgb2=RgbColor(0,0,0);
HsbColor hsb1=HsbColor(0,0,0);
HsbColor hsb2=HsbColor(0,0,0);
HslColor hsl1=HslColor(0,0,0);
HslColor hsl2=HslColor(0,0,0);

uint32_t times=1;
volatile bool playingEffect = false;
uint32_t effectCounter=0;

uint32_t offset=0;

WiFiManager wifiManager;

void setup() {
#ifdef ESP8266
  if (drd.detectDoubleReset()) { //if user double clicks reset button, then reset wifisetting
    wifiManager.resetSettings();
    drd.stop();
  }
#endif
  ////////////////// A whole bunch of initialization stuff that prints no matter what.
  Serial.begin(115200);
  if (DEBUG_MODE) {
    Serial.println();
    Serial.println();
    Serial.println(F("Serial started"));
#ifdef ESP8266
    Serial.println(F("Detected ESP8266..."));
#else
    Serial.println(F("Defaulting to ESP32..."));
    esp_wifi_set_ps(WIFI_PS_NONE); // No power save mode
#endif
  }
  ed.setCompile(String(__TIME__));    //Compiling erases variables previously changed over the network
  ed.start();
 
  setUdpPacketSize();
  
  //Initializes NeoPixelBus
  startNeoPixelBus();
  
  // We start by connecting to a WiFi network
  //Serial.print("Connecting to ");
  //Serial.println(ssid);
 // WiFi.mode(WIFI_STA);  // WIFi STATION mode only
  //WiFi.begin(ssid, pass);
  //WiFi.config(local_ip, gateway, subnet);

  //Animate from dark to initial color in 3 seconds on module power up
  initDisplay();

  // Set milliamps value
  milliAmpsLimit = amps * 1000;

  wifiManager.autoConnect("Enviral");

  if (DEBUG_MODE) {
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());
  }
  //SPIFFS Setup access file system
#ifdef ESP8266
  if (!SPIFFS.begin()) {
#else
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
#endif
    Serial.println(F("Failed to mount file system"));
    while(1) {
      delay(1000);
    }
  }

  // access by local dns
  MDNS.begin(host);
  
  startUDP();
  if (DEBUG_MODE) {
    Serial.print(F("Local port: "));
#ifdef ESP8266
    Serial.println(udp.localPort());
#else
    Serial.println(udpPort);
#endif
    Serial.print(F("Expected packagesize:"));
    Serial.println(udpPacketSize);
  
    Serial.println(F("Setup done"));
    Serial.println(F("Opcodes"));
    Serial.print(F("Poll: "));Serial.println(POLL);
    Serial.print(F("PollReply: "));Serial.println(POLLREPLY);
    Serial.print(F("Update: "));Serial.println(UPDATEFRAME);
    Serial.println(F(""));
  }

  for(int i=0 ; i<framesToMonitor ; i++)  //blank all frames metadata
     framesMD[i].frame=0;

  // here we place all the different web services definition

  server.on("/survey", HTTP_GET, []() {
    // build Javascript code to draw SVG wifi graph
    IPAddress local_ip=WiFi.localIP();
    String rt=F("<!doctype html><html><body>");
    rt+=F("Connected to:");
    rt+=WiFi.SSID();
    rt+=F("<br>IP address:");
    rt+=String(local_ip[0]);
    rt+=F(".");
    rt+=String(local_ip[1]);
    rt+=F(".");
    rt+=String(local_ip[2]);
    rt+=F(".");
    rt+=String(local_ip[3]);
    rt+=F("<br>port:");
    rt+=String(udpPort);
    rt+=F("<br>Expected packet size:");
    rt+=String(udpPacketSize);
    rt+=F("<br><h2>WiFi monitoring</h2><svg id='svg' width='800' height='800'></svg><script type='text/javascript'>");
    rt+=F("var svgns = 'http://www.w3.org/2000/svg';var svg = document.getElementById('svg');");
    rt+=F("var color=0,colors = ['red','orange','blue','green','purple','cyan','magenta','yellow'];");
    rt+=F("line(80,400,640,400,'','');for(i=-1;i<=13;i++) {label='Ch'+i;if (i<1 || i>11 ) label='';line(i*40+120,400,i*40+120,80,null,label);}");
    rt+=F("for(i=-90;i<=-20;i+=10) {label=''+i+'dBm';line(80,-i*4,640,-i*4,label,null);}");
    rt+=getSSIDs();
    rt+=F("function line(x1,y1,x2,y2,lleft,ldown) {var l=document.createElementNS(svgns,'line');l.setAttribute('stroke','black');");
    rt+=F("l.setAttribute('x1', x1);l.setAttribute('y1', y1);l.setAttribute('x2', x2);l.setAttribute('y2', y2);svg.appendChild(l);");
    rt+=F("var t=document.createElementNS(svgns,'text');t.textContent = ldown;t.setAttribute('x', x1-15);t.setAttribute('y', y1+20);svg.appendChild(t);");
    rt+=F("var t=document.createElementNS(svgns,'text');t.textContent = lleft;t.setAttribute('x', x1-60);t.setAttribute('y', y1+5); svg.appendChild(t);}");
    rt+=F("function channel(ch,db,ssid) {var p=document.createElementNS(svgns,'path');");
    rt+=F("p.setAttribute('d', 'M'+((ch-2)*40+120)+' 400 l60 '+(-400-db*4)+' l40 0 L'+((ch+2)*40+120)+' 400');");
    rt+=F("p.setAttribute('stroke',colors[color]);p.setAttribute('stroke-width',3);p.setAttribute('fill','none');svg.appendChild(p);");
    rt+=F("var t=document.createElementNS(svgns,'text');t.setAttribute('stroke',colors[color]);t.textContent = ssid;");
    rt+=F("t.setAttribute('x', ch*40-(ssid.length/2*8)+120);t.setAttribute('y', -db*4-5);svg.appendChild(t); color=(color+1) % colors.length;}");
    rt+=F("</script></body></html>");
    server.send(200, textHTML, rt);
  });

  server.on("/getstatus", HTTP_GET, []() {
    // build Javascript code to draw SVG wifi graph
    IPAddress local_ip=WiFi.localIP();
    String rt=F("<!doctype html><html><body>");
    //rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt += F("Connected to:");
    rt += WiFi.SSID();
    rt += F("<br>IP address:");
    rt += String(local_ip[0]);
    rt += F(".");
    rt += String(local_ip[1]);
    rt += F(".");
    rt += String(local_ip[2]);
    rt += F(".");
    rt += String(local_ip[3]);
    rt += F("<br>port:");
    rt += String(udpPort);
    rt += F("<br>Expected packet size:");
    rt += String(udpPacketSize);
    rt += F("</body></html>");
    server.send(200, textHTML, rt);
  });
  
  
//  server.on("/mcu_info", HTTP_GET, []() {
//    // build javascript-like data
//    IPAddress local_ip=WiFi.localIP();
//
//    String rt = "name:"+deviceName;
//    rt += ",";
//    rt += "ip:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
//    rt += ",";
//    rt += "ssid:"+WiFi.SSID();
//    rt += ",";
//    rt += "port:"+String(udpPort);
//    rt += ",";
//    rt += "packetsize:"+String(udpPacketSize);
//    server.send(200, textHTML, rt);
//  });

  server.on("/mcu_json", HTTP_GET, []() {
    // build json data
    if (DEBUG_MODE) {
      Serial.println(F("Received GET on /mcu_json"));
    }
    String local_ip=WiFi.localIP().toString();
    String local_ssid=String(WiFi.SSID());
    String rt;
    StaticJsonBuffer<1024> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root[FPSTR(devNameJSON)] = deviceName;
    root[FPSTR(ipJSON)] = local_ip;//String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    root[FPSTR(ssidJSON)] = local_ssid;
    root[FPSTR(udpPortJSON)] = udpPort;
    root[FPSTR(packetSzJSON)] = udpPacketSize;
    root[FPSTR(chunkSzJSON)] = chunkSize;
    root[FPSTR(pixStripJSON)] = pixelsPerStrip;
    root[FPSTR(maPixelJSON)] = mAPerPixel;
    root[FPSTR(ampJSON)] = amps;
    JsonArray& wcArr = root.createNestedArray(FPSTR(initColJSON));
    wcArr.add(InitColor[0]);
    wcArr.add(InitColor[1]);
    wcArr.add(InitColor[2]);
#ifdef ESP8266
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {      
      root[FPSTR(totalBytesJSON)] = fs_info.totalBytes;
      root[FPSTR(usedBytesJSON)] = fs_info.usedBytes;
      root[FPSTR(blockSizeJSON)] = fs_info.blockSize;
      root[FPSTR(pageSizeJSON)] = fs_info.pageSize;
      root[FPSTR(maxOpenFilesJSON)] = fs_info.maxOpenFiles;
      root[FPSTR(maxPathLengthJSON)] = fs_info.maxPathLength;
    }
#endif
    root.printTo(rt);
    server.send(200, appJSON, rt);
  });

  server.on("/getframes", HTTP_GET, []() {
    String rt=F("<html><body><table>");
    rt+=F("<tr style=\"border:1px solid black\"><th>Frame<br>#</th><th>Action<br>byte</td><th>Arrived At<br>[ÂµS]</th><th>Packet Size<br>[bytes]</th><th>Power<br>[mA]</th><th>Power<br>Adjustment</th><th>Packet CPU<br>time [ÂµS]</th><th>Frame CPU<br>time [ÂµS]</th></tr>");
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
          rt += F("<tr align=\"center\" bgcolor=\"lightgrey\">");
        else 
          rt += F("<tr align=\"center\">");
        rt += F("<td>");
        rt += String(framesMD[i].frame);
        rt += F("</td>");
        rt += F("<td>");
        rt += String(framesMD[i].part);
        rt += F("</td>");
        rt += F("<td>");
        rt += String(framesMD[i].arrivedAt);
        rt += F("</td>"); 
        rt += F("<td>");
        rt += StringZ(framesMD[i].packetSize);
        rt += F("</td>");
        rt += F("<td>");
        rt += StringZ(framesMD[i].power);
        rt += F("</td>");
        rt += F("<td>");
        rt += StringZ(framesMD[i].adjustedPower);
        rt += F("</td>");
        rt += F("<td>");
        rt += StringZ(framesMD[i].processingTime);
        rt += F("</td>");
        if(framesMD[i].part==1) acum = framesMD[i].processingTime;
        if(framesMD[i].part>1)  acum = acum + framesMD[i].processingTime;
        if(framesMD[i].part==0)  {
          rt+=F("<td>");
          rt+=String(acum+framesMD[i].processingTime);
          rt+=F("</td>");
          acum=0;}
         else
          rt+=F("<td></td>");
         rt+=F("</tr>");
       };
    };
    rt+=F("</table></body></html>");
    server.send(200, F("text/html; charset=UTF-8"), rt);
  });


  server.on("/play", HTTP_POST, []() {
    if (DEBUG_MODE) {
      Serial.print(F("Received play POST "));Serial.println(server.arg(plain));
    }
    udp.stop();udp.flush();
    // <effect> [RGB[r1],[g1],[b1]] [RGB[r],[g2],[b2]] [T<times>] [F<frames>]
    // Executes "effect" with the specified parameters 
    // Ej: blink rgb255,0,0 rgb0,0,0 t10 f10
    // Blinks red / black for 10 times showing each color during 10 frames.
    play=server.arg(plain);  //retrieve body from HTTP POST request
    streaming=false; //quit streaming mode and enter effects playing mode into main loop
    playingSprite = false;  //Stop playing the sprite animation
    playingEffect=true;
    offset=0; // start parsing new line from leftmost character
    effectCounter = 0; // Reset the animation repeater
    animations.StopAll();
    //Serial.println("POST request");
    parseEffect();
    if (DEBUG_MODE) {
      Serial.println(F("Effect parsed"));
    }
    server.send(200,textPlain, F("OK"));
    });

  server.on("/mcu_json", HTTP_POST, []() {
    String updateString, cmd;
    StaticJsonBuffer<1536> jsonBuffer;
    char str[64];
    int val;
    float fval;
    byte v1, v2, v3;
    String rt;
    bool initD = false;

    // Need to stop udp in case we're streaming.
    udp.stop();udp.flush();
    
    if (server.hasArg(plain) == false) {
      server.send(422, appJSON, F("{\"error\":\"HTTP BODY MISSING\"}"));
      udp.begin(udpPort);
      return;
    }
    updateString = server.arg(plain);  //retrieve body from HTTP POST request
#ifdef ESP8266
    drd.stop(); //Prevents WiFi wiping during resets
#endif
    JsonObject& input = jsonBuffer.parseObject(server.arg(plain));
    if (!input.success()) {     //Not a well formed json. Checking for regular command
      
      if (updateString.length() > 64) {
        server.send(422, appJSON, F("{\"error\":\"COMMAND TOO LONG\"}"));
        udp.begin(udpPort);
        return;
      }
      
      updateString.toCharArray(str, 64);
      cmd = strtok(str, " ");
      if (cmd.indexOf(pixStripJSON) == 0) {
        blankFrame();
        val = String(strtok(NULL, " ")).toInt();
        
        if (!updatePixels(val)) {
          server.send(422, appJSON, F("{\"error\":\"PARAMETER OUT OF RANGE\",\"pixels_per_strip\":\"Failed\"}"));
          udp.begin(udpPort);
          return;
        }
        server.send(200,appJSON, F("{\"pixels_per_strip\":\"Success\"}"));
        startNeoPixelBus();
        initDisplay();
        
      } else if (cmd.indexOf(chunkSzJSON) == 0) {
        val = String(strtok(NULL, " ")).toInt();
        if (!updateChunk(val)) {
          server.send(422,appJSON, F("{\"chunk_size\":\"Failure\"}"));
          udp.begin(udpPort);
          return;
        }
        setUdpPacketSize();
        server.send(200,appJSON, F("{\"chunk_size\":\"Success\"}"));
        
      } else if (cmd.indexOf(maPixelJSON) == 0) {
        val = String(strtok(NULL, " ")).toInt();
        if (!updateMA(val)) {
          server.send(422,appJSON, F("{\"ma_per_pixel\":\"Failure\"}"));
          udp.begin(udpPort);
          return;
        }
        initDisplay();
        server.send(200,appJSON, F("{\"ma_per_pixel\":\"Success\"}"));
        
      } else if (cmd.indexOf(devNameJSON) == 0) {
        if (!updateName(String(strtok(NULL, " ")))) {
          server.send(422,appJSON, F("{\"device_name\":\"Failure\"}"));
          udp.begin(udpPort);
          return;
        }
        server.send(200,appJSON, F("{\"device_name\":\"Success\"}"));
        
      } else if (cmd.indexOf(ampJSON) == 0) {
        fval = String(strtok(NULL, " ")).toFloat();
        if (!updateAmps(val)) {
          server.send(422,appJSON, F("{\"amps_limit\":\"Failure\"}"));
          udp.begin(udpPort);
          return;
        }
        initDisplay();
        server.send(200,appJSON, F("{\"amps_limit\":\"Success\"}"));
        
      } else if (cmd.indexOf(udpPortJSON) == 0) {
        val = String(strtok(NULL, " ")).toInt();
        if (!updateUDP(val)) {
          server.send(422,appJSON, F("{\"udp_streaming_port\":\"Failure\"}"));
          udp.begin(udpPort);
          return;
        }
        startUDP();
        server.send(200,appJSON, F("{\"udp_streaming_port\":\"Success\"}"));
        
      } else if (cmd.indexOf(initColJSON) == 0) {
        v1 = String(strtok(NULL, " ")).toInt();
        v2 = String(strtok(NULL, " ")).toInt();
        v3 = String(strtok(NULL, " ")).toInt();
        if (!updateWarmUp(v1, v2, v3)) {
          server.send(422,appJSON, F("{\"warmup_color\":\"Failure\"}"));
          udp.begin(udpPort);
          return;         
        }
        initDisplay();
        server.send(200,appJSON, F("{\"warmup_color\":\"Success\"}"));
        
      } else {
        server.send(422,appJSON, F("{\"error\":\"INVALID COMMAND\"}"));
      } 
      
    } else {  //JSON detected
      JsonObject& root = jsonBuffer.createObject();

      blankFrame();

      if (input[FPSTR(pixStripJSON)] != NULL) {
        root[FPSTR(pixStripJSON)] = (updatePixels(input[FPSTR(pixStripJSON)]) ? FPSTR(successStr) : FPSTR(failStr));
        startNeoPixelBus();
        initD = true;
      }
      
      if (input[FPSTR(chunkSzJSON)] != NULL) {
        root[FPSTR(chunkSzJSON)] = (updateChunk(input[FPSTR(chunkSzJSON)]) ? FPSTR(successStr) : FPSTR(failStr));
        setUdpPacketSize();
      }
      
      if (input[FPSTR(maPixelJSON)] != NULL) {
        root[FPSTR(maPixelJSON)] = (updateMA(input[FPSTR(maPixelJSON)]) ? FPSTR(successStr) : FPSTR(failStr));
        initD = true;
      }
      
      if (input[FPSTR(devNameJSON)].success()) {
        root[FPSTR(devNameJSON)] = (updateName(String((const char*)input[FPSTR(devNameJSON)])) ? FPSTR(successStr) : FPSTR(failStr));
      }
      
      if (input[FPSTR(ampJSON)].success()) {
        root[FPSTR(ampJSON)] = (updateAmps(input[FPSTR(ampJSON)]) ? FPSTR(successStr) : FPSTR(failStr));
        
        initD = true;
      }
      
      if (input[FPSTR(udpPortJSON)] != NULL) {
        root[FPSTR(udpPortJSON)] = (updateUDP(input[FPSTR(udpPortJSON)]) ? FPSTR(successStr) : FPSTR(failStr));
      }
      
      if (input[FPSTR(initColJSON)] != NULL ) {
        v1 = input[FPSTR(initColJSON)][0];
        v2 = input[FPSTR(initColJSON)][1];
        v3 = input[FPSTR(initColJSON)][2];
        root[FPSTR(initColJSON)] = (updateWarmUp(v1, v2, v3) ? FPSTR(successStr) : FPSTR(failStr));
        initD = true;
      }

      if (input[FPSTR(frameTimeJSON)].success()) {
        udpMinFrameTime = input[FPSTR(frameTimeJSON)].as<uint16_t>();
        root[FPSTR(frameTimeJSON)] = FPSTR(successStr);
      }

      if (input[F("debug")].success()) {
        DEBUG_MODE = input[F("debug")].as<bool>();
      }

      if (input[F("pkt_debug")].success()) {
        PACKETDROP_DEBUG_MODE = input[F("pkt_debug")].as<bool>();
      }
      root.printTo(rt);
      if (initD) initDisplay();
      server.send(200, appJSON, rt);
    }

    udp.begin(udpPort);
   });

  // **** FILE SERVER HTTP ENDPOINTS **** //
  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, textPlain, F("FileNotFound"));
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, textPlain, "");
  }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, textPlain, F("FileNotFound"));
    }
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
#ifdef ESP8266 //TODO figure out how to get GPIO status on ESP32
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
#endif
    json += "}";
    server.send(200, textJSON, json);
    json = String();
  });
  
  // Start the server //MDB
  server.begin();
}

void loop() { //main program loop
  
  if (streaming && !playingSprite && !playingEffect) {
    opcode = parseUdpPoll();
  } else {      
    while(udp.parsePacket()) { // Throw away udp packets
      if (DEBUG_MODE) {
        Serial.println(F("UDP packet dropped"));
      }
    }
    opcode = NOPACKET;
  }  
  
  // opcodes between 0 and 99 represent the chunkID
  if (opcode <= CHUNKIDMAX && opcode >= CHUNKIDMIN) {
    playStreaming(opcode);
    
  } else if (opcode == UPDATEFRAME) {
    udpUpdateFrame();
    
  } else if (opcode == CONFIG) {
    udpConfigDevice();
    
  } else if (opcode == POLL) {
    udpSendPollReply();
    
  } else if (opcode == POLLREPLY) {
    //POLLREPLY safe to ignore
    
  } else if (playingEffect) {
    if (effectCounter < times) {
      animations.UpdateAnimations();
      strip->Show();
    } else {
      if (playingSprite) blankFrame();
      streaming=true;
      udp.begin(udpPort);
      playingEffect=false;
      playingSprite=false;
    }
    
  // Streaming but nothing received check timeout
  } else if (streaming && lastStreamingFrame!=0 && millis()-lastStreamingFrame>STREAMING_TIMEOUT*1000) {
      if (DEBUG_MODE) {
        Serial.println(F("Streaming timeout"));
      }
      blankFrame();
      lastStreamingFrame=0;
  }
  //server.handleClient();
}

void blankFrame() {
  paintFrame(RgbColor(0,0,0));
  strip->Show();
};

void paintFrame(RgbColor c) {
  //c=adjustToMaxMilliAmps(c); // do not allow to exceed max current
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

void parseEffect() {

  if (DEBUG_MODE) {
    Serial.println(play);
  }
  String line,params;
  int pos,RGBcolors,HSBcolors,HSLcolors;
  if(offset>=play.length()) { // when "play sequence" is finished had to go back to streaming mode
    times = 0;
  } else { // try to fetch next "play sequence" line
    line="";
    while (offset<play.length() && byte(play[offset])!=10) {  // extract line
      line+=play[offset++];
    }
    if(byte(play[offset])==10) offset++; // skip line feed

    command=getCommand(line);  //Parse all parameters
    if (command.equals("SPRITE")) {
      if (!handleSprite()) {
        Serial.println(F("Sprite command aborted"));
        effectCounter = 0;
        times = 0;
        Serial.flush();
        return;
      }
      return;
    } else if (command.equals("BLANK")) {
      blankFrame();
      times = 0;
      return;
    }
    params=getParams(line);
    // Get RBG colors
    RGBcolors=getColors(params,"RGB");
    if(RGBcolors==1) {
      rgb1=LastColor;
      rgb2=adjustToMaxMilliAmps(getRGB(params,1));
    } else {
      rgb1=adjustToMaxMilliAmps(getRGB(params,1));  //retrieve RGB parameter and adjust down to stay within power limit
      rgb2=adjustToMaxMilliAmps(getRGB(params,2));   //retrieve RGB parameter and adjust down to stay within power limit
    }
    if(RGBcolors>0) LastColor=rgb2;

    // Get HSB colors
    HSBcolors=getColors(params,"HSB");
    if(HSBcolors==1) {
      hsb1=LastColor;
      hsb2=adjustToMaxMilliAmps(getHSB(params,1));
    } else {
      hsb1=adjustToMaxMilliAmps(getHSB(params,1));
      hsb2=adjustToMaxMilliAmps(getHSB(params,2));
    }
    if(HSBcolors>0) LastColor=hsb2;

    // Get HSL colors
    HSLcolors=getColors(params,"HSL");
    if(HSLcolors==1) {
      hsl1=LastColor;
      hsl2=adjustToMaxMilliAmps(getHSL(params,1));
    } else {
      hsl1=adjustToMaxMilliAmps(getHSL(params,1));
      hsl2=adjustToMaxMilliAmps(getHSL(params,2));
    }
    if(HSLcolors>0) LastColor=hsl2;

    times=getTimes(params);
    uint32_t frames=getFrames(params);
    effectCounter = 0;
    uint32_t effectAnimationTime = 16.66667 * double(frames);   // ms per 1/60 sec * (seconds per animation)

    animations.StartAnimation(0, effectAnimationTime, LoopAnimUpdate);
    if (DEBUG_MODE) {
      Serial.println("command:" + command + " rgb1:" + rgb1.R+","+rgb1.G+","+rgb1.B + " rgb2:" + rgb2.R+","+rgb2.G+","+rgb2.B + " duration(frames):" + frames + " repetitions:" + times);
    }
  }
}

void playStreaming(int chunkID) {
  // New frame incoming check time
  if (chunkID == 0 && arrivedAt + udpMinFrameTime < millis()) {
    minFrameTimeMet = true;
  }
  
  if (!minFrameTimeMet) {
    return;
  }
  
  arrivedAt=millis();
  
  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("---Incoming---"));
    Serial.print(F("ChunkID: "));
    Serial.println(chunkID);

  }

  framesMD[frameIndex].part=chunkID;
  framesMD[frameIndex].arrivedAt=arrivedAt;
  
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
//  if (PACKETDROP_DEBUG_MODE) {
//    //ReplyBuffer[action] = 1;
//    ReplyBuffer[chunkID] = 1;
//  }

  framesMD[frameIndex].packetSize=0;
  framesMD[frameIndex].power=0;
  framesMD[frameIndex].adjustedPower=0;
  framesMD[frameIndex].processingTime=millis()-framesMD[frameIndex].arrivedAt;
  frameIndex=(frameIndex +1) % framesToMonitor;

  if (PACKETDROP_DEBUG_MODE) { // If Debug mode is on print some stuff
    Serial.println(F("--end of packet and stuff--"));
    Serial.println(F(""));
  }
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

void animBlink(RgbColor rgb1, RgbColor rgb2, float progress) {
  if (progress < 0.5) {
    paintFrame(rgb1);
  } else {
    paintFrame(rgb2);
  }
}

void animHue(RgbColor rgb1, RgbColor rgb2, float progress) {  
  paintFrame(RgbColor::LinearBlend(rgb1, rgb2, progress));
}

void animHue2(RgbColor rgb1, RgbColor rgb2, float progress) {
  if (progress < 0.5) {
    paintFrame(RgbColor::LinearBlend(rgb1, rgb2, progress * 2));
  } else {
    paintFrame(RgbColor::LinearBlend(rgb2, rgb1, (progress - 0.5) * 2));
  }
}

void animPulse(RgbColor rgb1, RgbColor rgb2, float progress) {
  if (progress < 0.5) {
    paintFrame(RgbColor::LinearBlend(rgb1, rgb2, progress * 2));
  } else {
    paintFrame(RgbColor(0,0,0));
  }
}

void animHueHsb(HsbColor hsb1, HsbColor hsb2, float progress) {
  // Linear blend on HSB colors then convert to RGB
  paintFrame(RgbColor(HsbColor::LinearBlend<NeoHueBlendShortestDistance>(hsb1, hsb2, progress)));
}

void animHueHsl(HslColor hsl1, HslColor hsl2, float progress) {
  // Linear blend on HSL colors then convert to RGB
  paintFrame(RgbColor(HslColor::LinearBlend<NeoHueBlendShortestDistance>(hsl1, hsl2, progress)));
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
//#ifdef ESP8266
//  strip = new NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> (pixelsPerStrip);
//#else
  strip = new NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod>(pixelsPerStrip, PixelPin);
//#endif
  ledDataBuffer = (RgbColor *)malloc(pixelsPerStrip);
  
  strip->Begin();
  delay(16);
}

void setUdpPacketSize() {
  if (packetBuffer) free(packetBuffer);
  //UDP max packet size is the UDP ID + OPCODE + ( RGB[chunksize][3] OR Update size )
  // Update size MAX_NAME_LENGTH + sizeof(PixelsPerStrip, ChunkSize, UdpPort, AmpsLimit, MaPerPixel, WarmUpColor)
  udpPacketSize = ( IDLENGTH + 1 + std::max( (chunkSize*3), (MAX_NAME_LENGTH  + 13) ) );
  packetBuffer = (byte *)malloc(udpPacketSize);//buffer to hold incoming and outgoing packets
}

void startUDP() {
  if (DEBUG_MODE) {
    Serial.println(F("Starting UDP"));
  }
  udp.stop();udp.flush();
  delay(100);
  udp.begin(udpPort);
}

void initDisplay() {  
  milliAmpsCounter = 0;
  millisMultiplier = 0;
  InitialColor=RgbColor(InitColor[0],InitColor[1],InitColor[2]); 
  InitialColor=adjustToMaxMilliAmps(InitialColor);
  for(int i=0;i<=90;i+=2) {
    paintFrame(RgbColor(InitialColor.R*i/90.0,InitialColor.G*i/90.0,InitialColor.B*i/90.0));
    strip->Show();
    delay(32);
  };
}

void LoopAnimUpdate(const AnimationParam& param) {
  // wait for this animation to complete,
  // we are using it as a timer of sorts
  if (param.state == AnimationState_Completed)
  {
      // done, time to restart this position tracking animation/timer
      animations.RestartAnimation(param.index);

      if (playingSprite) {
        uint16_t repeats = pixelsPerStrip / spritePixels;
        // If there are less pixels in the strip then the sprite do it at least once
        if (repeats == 0) {
          repeats = 1;
        // If the pixels in the strip are not evenly divisibly by the sprite pixels then repeat one more time to fill
        } else if (pixelsPerStrip % spritePixels) {
          repeats += 1;
        }
        
        // draw the frame and repeat accross all pixels
        for (int i=0; i < repeats; i++) {
          spriteSheet->Blt(*strip, i*spritePixels, indexSprite);
        }
        
        indexSprite = (indexSprite + 1) % spriteFrames; // increment and wrap
        if (indexSprite == 0) {
          effectCounter++;
        }
      } else {
        if (DEBUG_MODE) {
          Serial.println(F("Incrementing Effect"));
        }
        effectCounter++;
      }
  }

  // Animator determines which animation to run

  if (!playingSprite) {
    if (DEBUG_MODE) {
      Serial.print(F("Progress"));Serial.println(param.progress);  
    }
    if (command == "BLINK"){
      animBlink(rgb1, rgb2, param.progress);
    } else if (command == "HUE") {
      animHue(rgb1, rgb2, param.progress);
    } else if (command == "HUE2") {
      animHue2(rgb1, rgb2, param.progress);
    } else if (command == "PULSE") {
      animPulse(rgb1, rgb2, param.progress);
    } else if (command == "HUEHSB") {
      animHueHsb(hsb1, hsb2, param.progress);
    } else if (command == "HUEHSL") {
      animHueHsl(hsl1, hsl2, param.progress);
    }
  }

}

// BiLinear Blend: 
//  Upper Left: cStart 
//  Lower Right: cEnd
//  Upper Right & Lower Left: value in file
bool loadSpriteFile(File f, uint8_t cStartIn[], uint8_t cEndIn[]) {

  // Read the headers in the file
  uint8_t tbuf[4];

  if (f.size() < 4) {
    Serial.println(F("File too small"));
    return false;
  }
  
  f.read(tbuf, 4);

  // Header1 holds num of pixels in sprite frame
  spritePixels = tbuf[0] * 256 + tbuf[1];

  // Header2 holds num of frames in sprite file
  spriteFrames = tbuf[2] * 256 + tbuf[3];

  // Reset milliamps counter
  float ma = 0;

  if (DEBUG_MODE) {
    String imgParams = "Frames: " + String(spriteFrames);
    imgParams += ", Start: (" + String(cStartIn[0]) + ", " + String(cStartIn[1]) + ", " + String(cStartIn[2]) + ")";
    imgParams += ", End: (" + String(cEndIn[0]) + ", " + String(cEndIn[1]) + ", " + String(cEndIn[2]) + ")";
    imgParams += ", SpritePixels: " + String(spritePixels) + ", BufferSize: " + String(bytesPerPixel * spritePixels * spriteFrames);
    Serial.println(imgParams);
  }
  uint8_t rgb[bytesPerPixel];

  unsigned long fileSize = 4 + (bytesPerPixel * spritePixels * spriteFrames);

  if (fileSize > MAX_SPRITESIZE) {
    Serial.println(F("SpriteFile exceeds maximum allowed"));
    return false;
  }

  // Check if file is of correct size
  if (f.size() < fileSize) {
    Serial.println(F("File too small"));
    return false;
  }
  
  free(imageBuffer);
  imageBuffer = (uint8_t *)malloc(bytesPerPixel * spritePixels * spriteFrames);
  
  uint16_t bufferCount = 0;
  float progress = 0;

  RgbColor cStart = RgbColor(cStartIn[0], cStartIn[1], cStartIn[2]);
  RgbColor cEnd = RgbColor(cEndIn[0], cEndIn[1], cEndIn[2]);

  // Prevents blending if no values are passed
  bool shouldBlend = (cStart.R || cStart.G || cStart.B || cEnd.R || cEnd.G || cEnd.B);

  for (uint16_t frameCounter = 0; frameCounter < spriteFrames; frameCounter++) {
    // Calc progress along frames
    progress = double(frameCounter) / double(spriteFrames-1);

    for (uint16_t i = 0; i < spritePixels; i++) {
      // If i is greater than the number of pixels given in the file then repeat previous information
      
      f.read(rgb, bytesPerPixel);
      if (DEBUG_MODE) {
        Serial.println("Pre_rgb: " + String(rgb[0]) + ", " + String(rgb[1]) + ", " + String(rgb[2]));
      }
      //If any of them have a color value then blend the transition colors
      if ( 
            shouldBlend &&                                                           // The user wants frames to blend
            (rgb[0] || rgb[1] || rgb[2]) &&                                          // The sprite file pixel has value in it
            (frameCounter != 0 || (cStart.R || cStart.G || cStart.B)) &&             // On first frame and initial blend has value
            (frameCounter + 1 != spriteFrames || (cEnd.R || cEnd.G || cEnd.B))             // On last frame and last blend has value
         ) { 
        RgbColor spriteColor = RgbColor(rgb[0], rgb[1], rgb[2]);
        RgbColor result = RgbColor::BilinearBlend(
          cStart,                 //Upper Left
          spriteColor,            //Upper Right
          spriteColor,            //Lower Left
          cEnd,                   //Lower Right
          progress,               //X axis
          progress                //Y axis
        );
        
        rgb[1] = result.G;
        rgb[0] = result.R;
        rgb[2] = result.B;

        if (DEBUG_MODE) {
          Serial.println("post_rgb: " + String(rgb[0]) + ", " + String(rgb[1]) + ", " + String(rgb[2]));      
        }
      }

      ma += (mAPerPixel/3) * (rgb[0] + rgb[1] + rgb[2]) / 255.0;
      
      imageBuffer[bufferCount++] = rgb[1];
      imageBuffer[bufferCount++] = rgb[0];
      imageBuffer[bufferCount++] = rgb[2];
      
    }

    if (DEBUG_MODE) {
        Serial.print(F("Raw_mA"));Serial.println(ma);
    }
    ma *= ceil((float)pixelsPerStrip / (float)spritePixels);

    // If pixels output a greater ma then adjust each pixel in the frame
    if (ma > milliAmpsLimit) {
      float spriteMillisMultiplier = milliAmpsLimit / ma;
      // Go back to start of frame
      bufferCount = bufferCount - (spritePixels * bytesPerPixel);
      for (uint16_t i = 0; i < spritePixels; i++) {
        for (byte j = 0; j < bytesPerPixel; j++) {
          imageBuffer[bufferCount] = floor(imageBuffer[bufferCount] * spriteMillisMultiplier);
          bufferCount++;
        }
      }
      if (DEBUG_MODE) {
        Serial.print(F("Calc'dMillis"));Serial.println(ma);
        Serial.print(F("MilliAmpsLimit"));Serial.println(milliAmpsLimit);
        Serial.print(F("SpriteMillisMultiplier"));Serial.println(spriteMillisMultiplier);
      }
    }

    // Reset mA for next frame
    ma = 0.0;
  }
    
  delete spriteSheet;
  spriteSheet = new NeoVerticalSpriteSheet<NeoBufferMethod<NeoGrbFeature>>(spritePixels, spriteFrames, 1, imageBuffer);
  return true;
}

// Parses a sprite command and returns true if it follows a valid format
bool spriteParse() {
  char buf[256];
  char *tok;
  if (DEBUG_MODE) {
    Serial.print("Sprite to parse: ");
    Serial.println(play);
  }
  //Make sure all arguments are present
  int rgbCount = 0;
  int sCount = 0;
  int tCount = 0;
  int fCount = 0;
  int i = 0;
  int commaCount = 0;
  play.toCharArray(buf, 256);

  tok = strtok(buf, " ");  //Assumes SPRITE and discards it

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
      default:
        return false;
    }
  }

  if (rgbCount == 2 && sCount == 1 && tCount == 1 && fCount == 1) return true;
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

// T variable is for number of times the animation runs
// RGB variables transition the pixels from one color to the next across the frames
// F is the time for the entire animation to take
// S is for the sprite number to play or a filename enclosed in single quotes
bool handleSprite() { //SPRITE rgb255,0,0 rgb0,0,255 t10 f30 s1 x4 y3 a60
                      //SPRITE rgb255,0,0 rgb0,0,255 t10 f30 s'filename.bmp' x8 y32 a60
  if (!spriteParse()) {
    Serial.println(F("Sprite failed to parse"));
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
  uint32_t timePerAnimation = 0;
  int8_t fileNum = -1;
  
  int i = 0;
  int tI = 0;
  int cI = 0;
  
  play.toCharArray(buf, 256);
  tok = strtok(buf, " ");     //Discards command SPRITE
 
  while (tok = strtok(NULL, " ")) {
    
    char c = tok[0];
    switch (c) {
      case 'r':
        char temp[12];
        i = 0;
        while (tok[3+i] != '\0') {// && i < 11) {
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
            } else {
              colorEnd[cI] = atoi(cTemp);
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
        } while (i < 12);
        firstColor = false;
        
        break;
      case 't':
        timeRepeats = getDigits(1, tok);
        break;
      case 'f':
        timePerAnimation = getDigits(1, tok);
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
      default:
        return false;
    }
  }

  if (fileNum > 0) {
    filename += "sp" + String(fileNum);
  }
  if (DEBUG_MODE) {
    Serial.print(F("Filename: "));Serial.println(filename);
  }
  // If same file loaded before just replay sprite unless color transition changed
  if (filename != spritelastfile || !compareArrays(colorStart, lastColorStart, bytesPerPixel) || !compareArrays(colorEnd, lastColorEnd, bytesPerPixel)) {
    spritelastfile = ""; // Reset last file
    File f = SPIFFS.open(filename, "r");
    if (!f) {
      Serial.println(F("file open failed"));
      return false;
    }

    if (DEBUG_MODE) {
      Serial.print(F("First RGB: "));Serial.println(String(colorStart[0]) + ":" + String(colorStart[1]) + ":" + String(colorStart[2]));
      Serial.print(F("Second RGB: "));Serial.println(String(colorEnd[0]) + ":" + String(colorEnd[1]) + ":" + String(colorEnd[2]));
    }
    if (!loadSpriteFile(f, colorStart, colorEnd)) {
      Serial.println(F("Failed to convert bitmap into memory"));
      Serial.flush();
      f.close();
      return false;
    }
    spritelastfile = filename;
    for (byte i = 0; i < bytesPerPixel; i++) {
      lastColorStart[i] = colorStart[i];
      lastColorEnd[i] = colorEnd[i];
    }

    f.close();
  }
  blankFrame();
  playingSprite = true;
  indexSprite = 0;
  effectCounter = 0;
  uint32_t spriteAnimationTime = 16.66667 * double(timePerAnimation) / double(spriteFrames);   // ms per 1/60 sec * (seconds per animation) / (Frames per animation)
  times = timeRepeats;
  animations.StartAnimation(0, spriteAnimationTime, LoopAnimUpdate);
  return true;
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
    Serial.println(F("Updating Frame"));
    Serial.flush();
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
//  if (PACKETDROP_DEBUG_MODE) {
//    // set the last byte of the reply buffer to 2, indiciating that the frame was sent to leds.
//    ReplyBuffer[sizeof(ReplyBuffer) - 1] = 2;
//    ReplyBuffer[0] = counterHolder;
//    counterHolder += 1;
//    // write out the response packet back to sender!
//    udp.beginPacket(udp.remoteIP(), UDP_PORT_OUT);
//    // clear the response buffer string.
//    for (byte i = 0; i < sizeof(ReplyBuffer); i++) {
//      udp.write(ReplyBuffer[i]);
//      ReplyBuffer[i] = 0;
//    }
//    udp.endPacket();
//  }

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

//***** FILE SERVER FUNCTIONS ******//
//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg(F("download"))) {
    return F("application/octet-stream");
  } else if (filename.endsWith(F(".htm"))) {
    return textHTML;
  } else if (filename.endsWith(F(".html"))) {
    return textHTML;
  } else if (filename.endsWith(F(".css"))) {
    return F("text/css");
  } else if (filename.endsWith(F(".js"))) {
    return F("application/javascript");
  } else if (filename.endsWith(F(".png"))) {
    return F("image/png");
  } else if (filename.endsWith(F(".gif"))) {
    return F("image/gif");
  } else if (filename.endsWith(F(".jpg"))) {
    return F("image/jpeg");
  } else if (filename.endsWith(F(".ico"))) {
    return F("image/x-icon");
  } else if (filename.endsWith(F(".xml"))) {
    return F("text/xml");
  } else if (filename.endsWith(F(".pdf"))) {
    return F("application/x-pdf");
  } else if (filename.endsWith(F(".zip"))) {
    return F("application/x-zip");
  } else if (filename.endsWith(F(".gz"))) {
    return F("application/x-gzip");
  }
  return textPlain;
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server.uri() != F("/edit")) {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (spritelastfile.equals(filename)) {
      // Force sprite reload
      spritelastfile = "";
    }
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
  }
}

void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, textPlain, F("BAD ARGS"));
  }
  String path = server.arg(0);
  if (path == "/") {
    return server.send(500, textPlain, F("BAD PATH"));
  }
  if (!SPIFFS.exists(path)) {
    return server.send(404, textPlain, F("FileNotFound"));
  }
  SPIFFS.remove(path);
  server.send(200, textPlain, "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, textPlain, F("BAD ARGS"));
  }
  String path = server.arg(0);
  if (path == "/") {
    return server.send(500, textPlain, F("BAD PATH"));
  }
  if (SPIFFS.exists(path)) {
    return server.send(500, textPlain, F("FILE EXISTS"));
  }
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, textPlain, F("CREATE FAILED"));
  }
  server.send(200, textPlain, "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, textPlain, F("BAD ARGS"));
    return;
  }

  String path = server.arg("dir");
  String output = "[";
#ifdef ESP8266
  Dir dir = SPIFFS.openDir(path);
  path = String();
  
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += F("{\"type\":\"");
    output += (isDir) ? F("dir") : F("file");
    output += F("\",\"name\":\"");
    output += String(entry.name()).substring(1);
    output += F("\",\"size\":");
    output += String(entry.size());
    output += F("}");
    entry.close();
  }
#else

  File dir = SPIFFS.open(path);
  if (!dir) {
    server.send(400, textPlain, F("Failed to open"));
    return;
  }
  if (!dir.isDirectory()) {
    server.send(400, textPlain, F("Not a directory"));
    return;
  }

  File file = dir.openNextFile();
  while (file) {
    if (output != "[") {
      output += ",";
    }
    bool isDir = file.isDirectory();
    output += F("{\"type\":\"");
    output += (isDir) ? F("dir") : F("file");
    output += F("\",\"name\":\"");
    output += String(file.name()).substring(1);
    output += F("\",\"size\":");
    output += String(file.size());
    output += F("}");
    file.close();
    file = dir.openNextFile();
  }

#endif
  output += "]";
  server.send(200, textJSON, output);
}
