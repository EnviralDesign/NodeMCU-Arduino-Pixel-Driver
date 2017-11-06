# TODO list - Features Roadmap

## 1) Expose LED configuration settings to user.
1. The plan should be to create another POST message system like we have now with /play.

  To recap, currently the user can send animation commands to `192.168.1.xxx/play` as a POST message.

  Ideally we should have a new address like this:

  `192.168.1.xxx/mcu_config` that we send a string containing whatever info we want to update in memory. That string might look like this:

  `updateString = "name:someDescriptiveName,ledsPerStrip:100,chunkSize:25,maPerPixel:60'`

  This string would then be received, then parsed by the nodeMCU and each variable updated and stored to **NVM (non volatile memory)** so that it persists when powered off and back on like the wifi settings are currently.

## 2) An addition to amperage calculation needs to be created:

  1. To recap, currently the total amps in use is being calculated from the total number of pixels, and the real-time color values of each led.

  If the amps exceeds the limits set by the user, the overall brightness of the strip is dialed back to keep it to the safe range.

  2. The addition to this system is a new variable for **mA per pixel**. Currently each led (red, green, and blue value) when at max brightness == 60mA. However depending on the led voltage (12v in the future) OR if the user has two strips wired in parallel, they need a way to adjust how many milliAmps each led should cost so their calculations are performed correctly.

  This would need to be defined and integrated into the current programming so that it is also evaluated. (right now 60mA is hardcoded)


## 3) Create support for NEW pixel mapped animation effects and configuration

  1. To recap, currently there are 4 effects that you can send to the nodeMCU as a command. these are light weight and happen through http POST.

  However, the current animation types set the color of the entire panel to a constant over time. These new animation effects would use pixel mapping to generate more interesting effects.

  2. There are lots of interesting developments and helper objects built into this led library for mapping and animation, so some R&D and testing with these would be the first step.
  https://github.com/Makuna/NeoPixelBus/wiki/Matrix-Panels-Support
  https://github.com/Makuna/NeoPixelBus/wiki/Raster-Image-Support

  3. Lastly, once we have support for these new types of effects, there will likely be a few more parameters we want to allow the user to configure.

  Are their leds in a grid formation? or a strip? How is the panel or fixture rotated?
  These are all configurations supported in the neoPixelBus library so it would make sense to expose these to the user as well through `/mcu_config`.
