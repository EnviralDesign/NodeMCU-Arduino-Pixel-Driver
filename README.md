# PxlNode-8266
---
## What this project is about:

**PxlNode-8266** is a flexible lightweight wifi based pixel controller designed around the **ESP-8266** development board.

It has a very constrained use case currently. The two biggest factors are:
- Currently only supports neopixel aka ws2811 / ws2812b etc style led.
- Configured for 1 strip output, using the nodeMCU's hardware acellerated DMA output for fast refresh rates.

What makes this library special is it's innate support for 2 very different modes of operation:

- **Streaming mode**
  - Uses a simple and straight forward UDP based protocol for streaming pixel data from a source device.
  - Current tests have achieved a consistent 60 fps @ 512 pixels.
  - **TouchDesigner/nodeMCU_softwareTestingPlatform.toe**


- **Command Mode**
  - Uses a very lightweight messaging protocol utilizing http requests with POST / GET.
  - Allows for IOT friendly, minimal programming to control the nodeMCU / leds with very little strain on networks etc.
  - **TouchDesigner/scanFornodeMCUs.toe**


- **Supported Platforms** that are currently being developed for:
    - Touch Designer (*for power users - pc and mac*)
    - Node-Red on Raspberry Pi (*IOT, also good for shared web based access*)
    - Android app (*for casual users, personal projects, and fully wireless setups*)



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


1. Register on [Derivative.ca]
2. [Download the installer] for the latest 099 x64 build: https://www.derivative.ca/099/Downloads/experimental.asp
3. Open Touch Designer, and perform the 1 time activation / login:
![alt tag](http://www.enviral-design.com/downloads/loginToTouch.jpg)
4. Open and run **TouchDesigner/nodeMCU_softwareTestingPlatform.toe** or **TouchDesigner/scanFornodeMCUs.toe** (More info on these files later)

### USAGE:

Currently this platform only supports 1 strip of pixels connected to the hardware acellerated pin of the nodeMCU. **GPIO3 aka RXD0** seen below - fourth from the bottom, on the right.
![alt tag](https://pradeepsinghblog.files.wordpress.com/2016/04/nodemcu_pins.png?w=616)

I've been using arduino 1.8.2. You'll need to get the latest libraries installed.
- DoubleResetDetector
- NeoPixelBus
- WiFiManager

First thing when you power on your nodeMCU, double press the reset button seen above (left of usb port) which will force reset of the internal SSID params. Open up your wifi menu on your phone or wifi enabled computer and connect to **"Enviral"**

There you can configure your device as intended. There will be more params here that are led specific but for now this is self explanatory.

from here, your nodeMCU will reset on its own, and "Enviral" will dissapear, and you can now find your ip address using the scan feature in the **TouchDesigner/scanFornodeMCUs.toe** file.

Once you have your ip adress, you can send POST commands from the file mentioned above as well. More info will be provided on this at a later time, but all code in files above is python and should be easy enough to understand if you have some coding experience.



### Up to date R&D Q/A

- https://docs.google.com/document/d/15fohtsI8zHB3XPj2QLFtR6nHrb2y4t7B38Z7AyHkmPo/edit?usp=sharing


### Working Examples

* 2 x 512 pixel demo @ 60 fps
https://www.youtube.com/watch?v=c9nZ8aAHsXQ

* 1 x 512 pixel battery powered demo @ 60 fps
https://www.youtube.com/watch?v=akc9gpH_Zqw


[Derivative.ca]: <http://www.derivative.ca/Login/RegisterForm.asp>
[Download the installer]: https://www.derivative.ca/088/Downloads/
