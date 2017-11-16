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

Once you have a command put together as a simple string, you'll send that to your PxlNode as a **POST** command where `plain='argument'` ( *not as raw post data* )

There is an ESP8266 bug where a soft reset peformed after a program flash but before a hard reset will cause the bootloader to hang. All of the mcu_config commands will initiate a soft reset. If you run into this issue, simply push the reset button or power cycle your NodeMCU.See https://github.com/esp8266/Arduino/issues/1017 for more informattion.
