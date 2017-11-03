# NodeMCU-Arduino-Pixel-Driver
This library is for the nodeMCU / esp8266. It enables the esp8266 to be universal wireless pixel controller with a wifi-configuration portal and support for both streaming per pixel protocol and a command based light-weight protocol.

You can find touch designer examples for driving the two modes below. There will be more general code based examples in the future.

1. **Streaming mode:**
  1. TouchDesigner/nodeMCU_softwareTestingPlatform.toe
2. **Command Mode:**
  1. TouchDesigner/scanFornodeMCUs.toe


### SETUP AND INSTALLATION:

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
