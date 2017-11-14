  - **192.168.1.xxx/mcu_config**
    - This command allows users to update pixels per strip, chunk size, and ma per pixel.
Command message's are structured like this:
- [ command ]
- [ values ]*

Some actual examples - any one of the lines below are a complete command:
```
all 100 25 60
pixels_per_strip 80
chunk_size 40
ma_per_pixel 120
```

Once you have a command put together as a simple string, you'll send that to your PxlNode as a **POST** command where `update='argument'` ( *not as raw post data* )

It's recommended to reset your NodeMCU after flashing. Running the update command forces the ESP to reset which will cause the bootloader to hang if it has not been reset. If you run into this issue, simply push the reset button or power cycle your NodeMCU.
