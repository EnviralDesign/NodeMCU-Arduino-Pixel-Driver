// include some libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NeoPixelBrightnessBus.h>
#include <WiFiManager.h>
#include <DoubleResetDetector.h>

#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS); 


///////////////////// USER DEFINED VARIABLES START HERE /////////////////////////////

String tmpName = "testMCU";

// number of physical pixels in the strip. 
#define PIXELS_PER_STRIP 100

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
#define CHUNK_SIZE 25

//maximum numbers of chunks per frame in order to validate we do not receive a wrong index when there are communciation errors
#define MAX_ACTION_BYTE 4 

// Dynamically limit brightness in terms of amperage.
#define AMPS 3

// UDP port to receive streaming data on.
#define UDP_PORT 2390

///////////////////// USER DEFINED VARIABLES END HERE /////////////////////////////



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
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELS_PER_STRIP, PixelPin);
NeoGamma<NeoGammaTableMethod> colorGamma;

// holds chunksize x 3(chans per led) + 1 "action" byte
#define UDP_PACKET_SIZE ((CHUNK_SIZE*3)+1)
byte packetBuffer[ UDP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
RgbColor ledDataBuffer[ PIXELS_PER_STRIP];
byte r;
byte g;
byte b;

RgbColor InitialColor=RgbColor(255,255,255); //Set here the inital RGB color to show on module power up
RgbColor LastColor=RgbColor(0,0,0);  //hold the last colour in order to stitch one effect with the following.


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

// Default Settings
String settings="mAPerPixel 60 Amps 4 name Enviral topology RA 10 10";
int mAPerPixel,Amps;
String name,topology;

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

String rt;

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


  strip.Begin();  //start neopixel instance.
    
  // We start by connecting to a WiFi network
  //Serial.print("Connecting to ");
  //Serial.println(ssid);
 // WiFi.mode(WIFI_STA);  // WIFi STATION mode only
  //WiFi.begin(ssid, pass);
  //WiFi.config(local_ip, gateway, subnet);

  //Animate from dark to initial color in 3 seconds on module power up
  InitialColor=adjustToMaxMilliAmps(InitialColor);
  for(int i=0;i<=90;i++) {
    paintFrame(RgbColor(InitialColor.R*i/90.0,InitialColor.G*i/90.0,InitialColor.B*i/90.0));
    delay(16);
  };

  //while (WiFi.status() != WL_CONNECTED) {
  //  delay(500);
  //  Serial.print(".");
  //}
  //Serial.println("");

  wifiManager.autoConnect("Enviral");

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
  //strip.Begin();
  //for (uint16_t i = 0; i < PIXELS_PER_STRIP; i++) {
  //  strip.SetPixelColor(i, RgbColor(0, 0, 0));
  //  ledDataBuffer[i] = RgbColor(0, 0, 0);
  //}
  //strip.Show();
  blankTime=micros()-blankTime;

  for(int i=0 ; i<framesToMonitor ; i++)  //blank all frames metadata
     framesMD[i].frame=0;

  // here we place all the different web services definition

  server.on("/survey", HTTP_GET, []() {
    // build Javascript code to draw SVG wifi graph
    IPAddress local_ip=WiFi.localIP();
    rt="<!doctype html><html><body>";
//    rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="Connected to:"+WiFi.SSID()+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
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
    IPAddress local_ip=WiFi.localIP();
    rt="<!doctype html><html><body>";
    //rt+="Connected to:"+String(ssid)+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="Connected to:"+WiFi.SSID()+"<br>IP address:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt+="<br>port:"+String(UDP_PORT)+"<br>Expected packet size:"+String(UDP_PACKET_SIZE);
    rt+="</body></html>";
    server.send(200, "text/html", rt);
  });
  
  
  server.on("/mcu_info", HTTP_GET, []() {
    // build javascript-like data
    IPAddress local_ip=WiFi.localIP();
    rt = "name:"+tmpName;
    rt += ",";
    rt += "ip:"+String(local_ip[0]) + "." + String(local_ip[1]) + "." + String(local_ip[2]) + "." + String(local_ip[3]);
    rt += ",";
    rt += "ssid:"+WiFi.SSID();
    rt += ",";
    rt += "port:"+String(UDP_PORT);
    rt += ",";
    rt += "packetsize:"+String(UDP_PACKET_SIZE);
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
};

void paintFrame(RgbColor c) {
  //c=adjustToMaxMilliAmps(c); // do not allow to exceed max current
  for (uint16_t i = 0; i < PIXELS_PER_STRIP; i++) strip.SetPixelColor(i, c);
  strip.Show();
};

RgbColor adjustToMaxMilliAmps(RgbColor c) {
  float ma=20*(c.R+c.G+c.B)/255.0*PIXELS_PER_STRIP;
  RgbColor r=c;
  if (ma > milliAmpsLimit)  {// need to adjust down
    r.R=c.R*milliAmpsLimit/ma;
    r.G=c.G*milliAmpsLimit/ma;
    r.B=c.B*milliAmpsLimit/ma;
  }
  // Serial.println("milliAmpsLimit:"+String(milliAmpsLimit)+" ma:"+String(ma));
  // Serial.println("adjustToMaxMilliAmps :"+String(c.R)+" "+String(c.G)+" "+String(c.B)+" -> "+String(r.R)+" "+String(r.G)+" "+String(r.B));
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
  if(pos>0) Amps=params.substring(pos+1).toInt();

  //get module name
  pos=params.indexOf("NAME");
  if(pos>=0) {
    while (params.substring(pos,pos+1)==" ") pos++; //skip spaces
    pos2=params.indexOf(" ",pos);
    if (pos2>=0) name=params.substring(pos,pos2);
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

