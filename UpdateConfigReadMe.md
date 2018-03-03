  - **192.168.1.xxx/mcu_config**
    - This command allows users to update pixels per strip, chunk size, and ma per pixel.
Command message's are structured like this:
- [ command ]
- [ values ]*

Some actual examples - any one of the lines below are a complete command:
```
device_name MyPixelNode
pixels_per_strip 80
chunk_size 40
ma_per_pixel 120
udp_streaming_port 3000
amps_limit 5.0
warmup_color 80 10 44
```
Alternatively, the command takes in a JSON object. It may include one, some, or all fields. Here is an example
```
{
    "device_name": "PxlNode-8266",
    "udp_streaming_port": 2390,
    "chunk_size": 64,
    "pixels_per_strip": 256,
    "ma_per_pixel": 60,
    "amps_limit": 3,
    "warmup_color": [
        200,
        75,
        10
    ]
}
```

Once you have a command put together as a simple string, you'll send that to your PxlNode as a **POST** command where `plain='argument'` ( *not as raw post data* ) or as a JSON

There are a few limitations:
`device_name` must not contain spaces and is limited to 64 characters.
`pixels_per_strip` cannot be greater than 1500.
`chunk_size` cannot be greater than 200.
`udp_streaming_port` cannot be greater than 65536
`warmup_color` no value can be greater than 255