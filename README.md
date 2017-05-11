# NodeMCU-Arduino-Pixel-Driver
This code base is still largely a WIP - will update this section as things come together more. Obviously use at your own risk ;)

### FEATURES:

1. Stream pre mapped pre generated led data to the nodeMCU using the simple protocol in the nodeMCU. (Working on examples and code for protocol, out soon.)

2. Stream up to ~512 pixels @ 60 fps per nodeMCU. More is possible if you're willing to sacrifice framerate.

3. Using built in /getstatus and /getframes diagnostic you can see how long the nodeMCU is spending per frame and per packet, and also see what the nodeMCU see's in terms of signal strength of the SSID's around it.

 Use the address syntax in chrome, or firefox: `<ipAddress/getframes>`

 - Example: `10.10.10.200/getstatus` or `10.10.10.200/getframes`

 - A brand new, experimental feature is being worked on that allows users to trigger built in animations instead of stream data using very light weight simple REST resource `<ipAddress/play>` followed by the animation parameters. (MORE ON THIS LATER)

### SETUP AND INSTALLATION:

1. Register on [Derivative.ca]

2. [Download the installer] for 088 x64 build  :62160

3. Open Touch Designer, and perform the 1 time activation / login:

 ![alt tag](http://www.enviral-design.com/downloads/loginToTouch.jpg)
4. Open and run **nodeMCU_softwareTestingPlatform.1.toe**

5. This is the home screen you want to be at:

 ![alt tag](http://www.enviral-design.com/blog/wp-content/uploads/2017/05/nodeMCU_driverSoftware.jpg)

 If you see a different screen, use the bookmarks to navigate to **HOME**

 ![alt tag](http://www.enviral-design.com/blog/wp-content/uploads/2017/05/nodeMCU_driverSoftware_homeScreen.jpg)

6. You can navigate to the **MAPPING** bookmark as well, to adjust the coordinates to match your leds:

 ![alt tag](http://www.enviral-design.com/blog/wp-content/uploads/2017/05/nodeMCU_driverSoftware_mappingScreen.jpg)

7. Also in **CONTENT** you can swap out, load, or create your own content to push to the leds instead of the default ramp I have setup:

 ![alt tag](http://www.enviral-design.com/blog/wp-content/uploads/2017/05/nodeMCU_driverSoftware_animations.jpg)

8. Lastly, if you're curious to see the python code being used to transmit data from touch designer to the nodeMCU's, check out **COMMUNICATION**:

 ![alt tag](http://www.enviral-design.com/blog/wp-content/uploads/2017/05/nodeMCU_driverSoftware_UDPOutcode.jpg)

 I wouldn't recommend changing this as it would likely cause the touch designer file to stop working correctly, but if you're comfortable with Touch, go ahead!

 The line, `udp.sendBytes(*op(x)[0].vals)` does the actual sending, I will expand on this at a later date, maybe with a more barebones example file or even a Processing example showing a more traditional codified version of transmission.



### Up to date R&D Q/A

- https://docs.google.com/document/d/15fohtsI8zHB3XPj2QLFtR6nHrb2y4t7B38Z7AyHkmPo/edit?usp=sharing


### Working Examples

* 8x nodeMCU's streaming pixel data from this touch designer file **(05/11/2017)** https://youtu.be/ZIMvriJ1oUo

* Update on the production of the nodeMCU PCB board **(04/10/2017)**
https://youtu.be/FQH2JhYvfZg

* 1 x 512 pixel battery powered demo @ 60 fps **(01-27-2017)**
https://youtu.be/akc9gpH_Zqw

* 2 x 512 pixel demo @ 60 fps **(12-13-2016)**
https://youtu.be/c9nZ8aAHsXQ


[Derivative.ca]: <http://www.derivative.ca/Login/RegisterForm.asp>
[Download the installer]: https://www.derivative.ca/088/Downloads/
