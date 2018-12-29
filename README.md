# PxlNode-8266

![alt tag](https://www.enviral-design.com/blog/wp-content/uploads/2017/11/cool-PxlNode.jpg)


## What this project is about:

**PxlNode-8266** is a flexible lightweight wifi based pixel controller designed around the **ESP-8266** development board.

The PxlNode is designed to work within the following parameters.
- Neopixel aka ws2811 / ws2812b etc style led.
- 1 strip output, using the nodeMCU's hardware accelerated DMA output for fast refresh rates.

Here are some of the main features. Read more about them in the wiki.

- **UDP Pixel Streaming protocol**
  - Uses a simple and straight forward UDP based protocol(more down below) for streaming raw pixel data from a client device.
  - **TouchDesigner/Streaming/StreamingExample_SIMPLE.toe**


- **POST based Lighting Command protocol**
  - Uses a very lightweight messaging protocol utilizing http requests with POST / GET.
  - Allows for IOT friendly, minimal programming to control the nodeMCU / leds with very little strain on networks etc.
  - Think Philips Hue style control of lights.
  - **TouchDesigner/Command/scanning_and_commands_SIMPLE.toe**


- **Client side UDP Network Scan**
  - Using a UDP broadcast with a specific byte array you can scan a network for all living PxlNode devices that are currently connected to the same network.


- **GET, POST, HTTP EndPoints**
  - **192.168.1.xxx**
    - HTTP: This home page displays memory use and pin status.
  - **192.168.1.xxx/survey**
    - HTTP: This command returns a visual graph that shows SSID's and their respective signal strength in the eyes of that particular PxlNode.
  - **192.168.1.xxx/getstatus**
    - HTTP: [TODO]
  - **192.168.1.xxx/mcu_info**
    - HTTP: [TODO]
  - **192.168.1.xxx/getframes**
    - HTTP: This command returns a list of several seconds of received frame diagnostic data. Useful to query this right after a stutter, or visual glitch to see where the problem might lie.
  - **192.168.1.xxx/edit**
    - HTTP: This page is where files may be uploaded, intended primarily for uploading / managing sprites.
  - **192.168.1.xxx/mcu_json**
    - GET: When this endpoint receives a GET request it returns information about the device in a JSON formatted string.
    - POST: When the endpoint receives a POST request, it is assumed that the message is a JSON string and will be parsed expecting certain variables.
  - **192.168.1.xxx/play**
    - POST: When the endpoint receives a POST command at this address it's expecting a command structured one of several ways. This can trigger a simple hue shift, or a sprite effect to play.


- **Dynamic brightness limitation**
  - Once you correctly configure your nodeMCU's number of pixels and amps limit, it will intelligently dim the entire strip when the calculated power consumption of the colors exceeds limits.
  - This allows for more creativity, and flexibility and mobility (battery powered low amp projects etc) with out sacrificing brightness when you don't need to.


- **Client Platforms in active development**:
  - **TouchDesigner** (*for power users - pc and mac*)
    - You would use this for show control, streaming, generating large amounts of pixel mapped data, etc.
    - Python examples exist within the TouchDesigner examples for those of you wanting to see the raw code, just open the .TOE files and look around.
  - **Android** (*for casual users, personal projects, and fully wireless setups*)
    - This is probably what most people will use for casual purposes. Does not support streaming.
    - Can scan for and configure PxlNode devices easily with this app.
    - Development nearing completion. Will be available on Google Play later in 2o19.



## Arduino IDE Setup:

1. Install Arduino IDE 1.8.2.
 - You can probably use later versions just fine but may need to troubleshoot any issues that come up between versions / libraries etc.
2. Go to boards manager, and install **esp8266** version **2.3.0** by *ESP8266 Community*
 - Probably good idea to use the latest version here.
3. When a nodeMCU is plugged in the computer should recognize it in device manager as a serial device on **COM 3**
4. You'll want to install some libraries. ultimately these will need to be included in your file:
```
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>
#include <WiFiManager.h>
#include <DoubleResetDetector.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <EnviralDesign.h>
```
**EnviralDesign.h** is a library you'll find in this repository. You'll want to copy it to



## Programming your nodeMCU:
[TODO] Document
At this point you can program the default Arduino code straight to the nodeMCU with out any changes, and configure everything you should need to change, via the wifi portal.

[TODO] Document Easy Installer process


## Touch Designer Setup:

1. Register on [Derivative.ca]
2. [Download the installer] for the latest 099 x64 build: https://www.derivative.ca/099/Downloads/experimental.asp
  - *I'm using the experimental 099 version and that's what these files are saved in.*
3. Open Touch Designer, and perform the 1 time activation / login.



## Wiring:

When using the esp8266, this platform only supports 1 strip of pixels connected to the hardware accelerated pin of the nodeMCU. **GPIO3 aka RXD0** seen below - fourth from the bottom, on the right.
![alt tag](https://pradeepsinghblog.files.wordpress.com/2016/04/nodemcu_pins.png?w=616)



## Connecting to a wireless network:

The first thing this software will want to do is connect to a wifi network. These two steps are what the nodeMCU tries to do when powered on:

1. Attempt to connect to the ssid/password stored in memory.
2. If it can't connect after a short period of time, it will go into "configuration" mode broadcasting it's own SSID named **Enviral**
  - You can also trigger "configuration" mode by double pressing the reset button if using the esp8266. (left of the usb port, as shown above)

If you're running this for the first time, you'll probably be greeted with the ssid **Enviral** either way:
![alt tag](https://www.enviral-design.com/blog/wp-content/uploads/2017/11/wifiscan_enviral.jpg)
Once configured and saved, your nodeMCU will reset on its own, and "Enviral" will disappear, and you can now start sending commands or pixel data to your nodeMCU.



## Streaming Pixel Data to the PxlNode:

To get started streaming, the easiest way to test your nodeMCU PxlNode is to run the included file below (you'll need to install TouchDesigner) and manually entering the IP address of your PxlNode and configure a few other params and you should see some moving colors showing up.

- **TouchDesigner\Streaming\StreamingExample_SIMPLE.toe**



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
huehsb hsb0,100,0 hsb0,100,100 f360 t1
```

Once you have a command put together as a simple string, you'll send that to your PxlNode as a **POST** command where `plain='argument'` ( *not as raw post data* )

The address you send the POST command to would look like this:
`192.168.1.xxx/play`




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
[Download the installer]: <https://www.derivative.ca/088/Downloads/>
