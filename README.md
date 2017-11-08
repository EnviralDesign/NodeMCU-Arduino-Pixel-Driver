# PxlNode-8266

![alt tag](https://www.enviral-design.com/blog/wp-content/uploads/2017/11/cool-PxlNode.jpg)


## What this project is about:

**PxlNode-8266** is a flexible lightweight wifi based pixel controller designed around the **ESP-8266** development board.

The PxlNode is currently designed to work with a specific led setup:
- Currently only supports neopixel aka ws2811 / ws2812b etc style led.
- Configured for 1 strip output, using the nodeMCU's hardware acellerated DMA output for fast refresh rates.

What makes this library special is it's innate support for 2 very different modes of operation, as well as a few other key features.

- **Streaming mode**
  - Uses a simple and straight forward UDP based protocol for streaming pixel data from a source device.
  - Current tests have achieved a consistent 60 fps @ 512 pixels.
  - **TouchDesigner/Streaming/StreamingExample_SIMPLE.toe**


- **Command Mode**
  - Uses a very lightweight messaging protocol utilizing http requests with POST / GET.
  - Allows for IOT friendly, minimal programming to control the nodeMCU / leds with very little strain on networks etc.
  - **TouchDesigner/Command/scanning_and_commands_SIMPLE.toe**


- **Client side Wi-fi Scan feature**
  - I'll talk more about this more down below, but through python, or Node-Red, or our Android app(in development) you'll be able to scan your wifi for connected PxlNode's and then use the collected data to communicate with multiple nodes.


- **GET / POST http request based communication**
  - **192.168.1.xxx/survey**
    - this command returns a visual graph that shows SSID's and their respective signal strength in the eyes of that particular PxlNode.
  - **192.168.1.xxx/mcu_info**
    - This command returns configuration information about your PxlNode. This is what other software that is scanning for PxlNodes should retrieve. The resulting data is easy to parse: `name:testMCU,ip:192.168.1.228,ssid:CampoGrande_24,port:2390,packetsize:76` more will be added to this return string as time goes on.
  - **192.168.1.xxx/play**
    - This is the address you send non streaming effect commands to.
  - **192.168.1.xxx/getframes**
    - this command returns a list of several seconds of received frame diagnostic data. Useful to query this right after a stutter, or visual glitch to see where the problem might lie.


- **Dynamic brightness limitation**
  - Once you correctly configure your nodeMCU's number of pixels and amps limit, it will intelligently dim the entire strip when the calculated power consumption of the colors exceeds limits.
  - This allows for more creativity, and flexibility and mobility (battery powered low amp projects etc) with out sacrificing brightness when you don't need to.


- **Client Platforms in active development**:
  - **Touch Designer** (*for power users - pc and mac*)
    - You would use this for show control, streaming, generating large amounts of pixel mapped data, etc.
  - **Node-Red on Raspberry Pi** (*IOT, also good for shared web based access*)
    - This is great for making web based access to scan and control lights. Very useful as well if you want an install that a lot of people can control with out needing to install software or apps etc.
  - **Android app** (*for casual users, personal projects, and fully wireless setups*)
    - This is probably what most people will use for casual purposes. Does not support streaming YET, development for command mode is first and nearing completion then phase 2 will include support for streaming / pixel mapping animations etc.



## Arduino IDE Setup:

1. Install Arduino IDE 1.8.2.
 - You can probably use later versions just fine but may need to troubleshoot any issues that come up between versions / libraries etc.
2. Go to boards manager, and install **esp8266** version **2.3.0** by *ESP8266 Community*
 - Probably good idea to use the latest version here.
3. When a nodeMCU is plugged in the computer should recognize it in device manager as a serial device on **COM 3**
4. You'll want to install some libraries. ultimately these will need to be included in your file:
```
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NeoPixelBrightnessBus.h>
#include <WiFiManager.h>
#include <DoubleResetDetector.h>
```



## Touch Designer Setup:

1. Register on [Derivative.ca]
2. [Download the installer] for the latest 099 x64 build: https://www.derivative.ca/099/Downloads/experimental.asp
  - *I'm using the experimental 099 version and that's what these files are saved in.*
3. Open Touch Designer, and perform the 1 time activation / login:
![alt tag](http://www.enviral-design.com/downloads/loginToTouch.jpg)



## Wiring:

Currently this platform only supports 1 strip of pixels connected to the hardware accelerated pin of the nodeMCU. **GPIO3 aka RXD0** seen below - fourth from the bottom, on the right.
![alt tag](https://pradeepsinghblog.files.wordpress.com/2016/04/nodemcu_pins.png?w=616)



## Programming your nodeMCU:

There are a few variables in the arduino code that are not yet exposed to the user through the wifi configuration portal. This is on my todo list and will be done soon but for now you'll need to configure a few things manually, before uploading your code.

```
///////////////////// USER DEFINED VARIABLES START HERE /////////////////////////////

String tmpName = "testMCU";

// number of physical pixels in the strip.
#define PIXELS_PER_STRIP 100

// This needs to be evenly divisible by PIXLES_PER_STRIP.
// This represents how large our packets are that we send from our software source IN TERMS OF LEDS.
#define CHUNK_SIZE 171

//maximum numbers of chunks per frame in order to validate we do not receive a wrong index when there are communciation errors
#define MAX_ACTION_BYTE 4

// Dynamically limit brightness in terms of amperage.
#define AMPS 3

// UDP port to receive streaming data on.
#define UDP_PORT 2390

///////////////////// USER DEFINED VARIABLES END HERE /////////////////////////////
```

- **tmpName**
  - Set this to a relevant name that makes sense for the nodeMCU. IE. "Kitchen", "Studio", "EntryWay" etc. It will show up in scan requests to help identify what that physical nodeMCU that IP address is attached to.
- **PIXELS_PER_STRIP**
  - Pretty straight forward, just enter in the number of pixels being driven by the nodeMCU. This variable is extremely important to several things, including the calculation of the dynamic brightness limitation.
- **CHUNK_SIZE**
  - CHUNK_SIZE is a little less intuitive - this setting pertains entirely to streaming mode. Ideally you'll want to strike a happy medium between as large as possible and modular and memorable. If you have a bunch of nodeMCU's driving 400 pixels each for instance, consider using a chunk size of 100. Going too much higher than 150-170 may result in flickering and weird results on the leds - I have not entirely figured out why but sticking with smaller numbers seems to be better.
- **MAX_ACTION_BYTE**
  - This variable will be automated in future versions. For now, set this number to the number of chunks that it will take to make up your entire strip.
  For example if you have a string of 100 pixels, and you set your chunk size to 25, you'd want to set MAX_ACTION_BYTE==4. (100 pixels / 25)
- **AMPS**
  - Assumes you know your voltage / current situation. If you step up or down your voltage for different led types outside of the scope of this document be sure to specify amps @ the voltage you're operating at. This info is usually on the power supply or battery etc.
- **UDP_PORT**
  - This port is what the nodeMCU assumes it will receive streaming pixel data through. Not usually necessary to change unless you have conflicts.



## Connecting to a wireless network:

The first thing this software will want to do is connect to a wifi network. These two steps are what the nodeMCU tries to do when powered on:

1. Attempt to connect to the ssid/password stored in memory.
2. If it can't connect after a short period of time, it will go into "configuration" mode broadcasting it's own SSID named **Enviral**
  - You can also trigger "configuration" mode by double pressing the reset button on the NodeMCU. (left of the usb port, as shown above)

If you're running this for the first time, you'll probably be greeted with the ssid **Enviral** either way:
![alt tag](https://www.enviral-design.com/blog/wp-content/uploads/2017/11/wifiscan_enviral.jpg)
Once configured and saved, your nodeMCU will reset on its own, and "Enviral" will disappear, and you can now start sending commands or pixel data to your nodeMCU.



## Streaming Pixel Data to the PxlNode:

There are 2 variables at this moment that you'll want to keep track of from your arduino code.

- PIXELS_PER_STRIP
- CHUNK_SIZE

Assuming you have those handy, you can stream data via UDP to a nodeMCU.
For now, I will simply point those who are curious to the fully working examples built in touch designer.

Touch Designer is a visual node-based programming environment so the examples should be clear enough that if you're comfortable with coding, you'll be able to piece together how it works from what's there. That said, I will be updating this readme with more complete documentation on how the streaming protocol is built.

You'll see a place to enter the two variables mentioned above - reccomend starting with the SIMPLE example.

- **TouchDesigner\Streaming\StreamingExample_SIMPLE.toe**
- **TouchDesigner\Streaming\StreamingExample_ADVANCED.toe**
  - Main difference in the advanced version, several things are automated and has support for many nodeMCU's.



## Controlling the PxlNode's LEDS via commands:

In many scenarios, you'll want to interact with the lights via commands. This would operate in very much the same way one would operate a Phillips Hue type product.

Command message's are structured like this:
- [ command ]
- [ startColor ]
- [ endColor ]
- [ repetitions ]
- [ frames per repetition ]

Some actual examples - any one of the lines below are a complete command:
```
blank
pulse rgb0,0,0 t10 f30 rgb32,0,0
blink rgb32,0,0 rgb0,0,32 t10 f20
hue rgb32,0,0 rgb0,0,32 t10 f60
hue2 rgb32,0,0 rgb0,0,32 t10 f30
huehsl hsl0,100,50 hsl359,100,50 f360 t1
huehsb hsl0,100,0 hsl0,100,100 f360 t1
```

Once you have a command put together as a simple string, you'll send that to your PxlNode as a **POST** command where `plain='argument'` ( *not as raw post data* )

The address you send the POST command to would look like this:
`192.168.1.xxx/play`

Currently all effects are constant colors applied to all leds in the array. Phase 2 of programming for the PxlNode will include other effect types that are spatially mapped!




## Working Examples:

* Ongoing R&D photo gallery
https://goo.gl/photos/gtSJMFyWE7NLGgep9

* 2 x 512 pixel demo @ 60 fps
https://www.youtube.com/watch?v=c9nZ8aAHsXQ

* 1 x 512 pixel battery powered demo @ 60 fps
https://www.youtube.com/watch?v=akc9gpH_Zqw




## Up to date R&D Q/A:

- https://docs.google.com/document/d/15fohtsI8zHB3XPj2QLFtR6nHrb2y4t7B38Z7AyHkmPo/edit?usp=sharing


## Website:
https://www.enviral-design.com





[Derivative.ca]: <http://www.derivative.ca/Login/RegisterForm.asp>
[Download the installer]: https://www.derivative.ca/088/Downloads/
