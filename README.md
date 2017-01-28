# NodeMCU-Arduino-Pixel-Driver

This code base is still largely a WIP - will update this section as things come together more.

---

Working Example:

2 x 512 pixel demo @ 60 fps
https://www.youtube.com/watch?v=c9nZ8aAHsXQ

1 x 512 pixel battery powered demo @ 60 fps
https://www.youtube.com/watch?v=akc9gpH_Zqw

With a more powerful router recently purchased, I was able to more or less stream to the 8 nodeMCU's I had at 60 fps but ran into some other problems:

- certain nodeMCU's freezing, seems to be related to something I've changed recently, or perhaps number of devices?
- [SOLVED] flickering on certain led panels : required a level shifter for the data line due to slightly lower voltage coming from 5v batteries than PSU's

