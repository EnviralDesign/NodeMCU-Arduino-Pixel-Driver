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
`udp_streaming_port` cannot be greater than 65535
`warmup_color` no value can be greater than 255

  - **192.168.1.xxx/mcu_json**
      - When this endpoint receives a GET request it returns information about the device in a JSON format.
Here's an example of what it would return.
```
{
    "device_name": "PxlNode-8266",
    "ip": "192.168.1.2",
    "ssid": "MySSID",
    "udp_streaming_port": 2390,
    "packetsize": 213,
    "chunk_size": 64,
    "pixels_per_strip": 640,
    "ma_per_pixel": 60,
    "amps_limit": 20,
    "warmup_color": [
        200,
        75,
        10
    ],
    "totalBytes": 957314,
    "usedBytes": 17068,
    "blockSize": 8192,
    "pageSize": 256,
    "maxOpenFiles": 5,
    "maxPathLength": 32
}
```

  - **192.168.1.xxx/edit**
      - This page is where files may be uploaded for playing as a sprite.

  - **192.168.1.xxx**
      - This home page displays memory use and pin status.

Some actual examples - any one of the lines below are a complete command:
```
blank
pulse rgb0,0,0 t10 f30 rgb32,0,0
blink rgb32,0,0 rgb0,0,32 t10 f20
hue rgb32,0,0 rgb0,0,32 t10 f60
hue2 rgb32,0,0 rgb0,0,32 t10 f30
huehsl hsl0,100,50 hsl359,100,50 f360 t1
huehsb hsb0,100,0 hsb0,100,100 f360 t1
sprite rgb0,0,255 rgb 255,0,255 t5 f60 s'filename.sprite'
sprite rgb0,0,255 rgb 255,0,255 t5 f60 s1
```

- **Sprite effect**
The sprite command needs to know which file you would like to play. To open one, place the name in single quotes following the s variable. If the filename is in quotes, the device will look in the root directory for a file with that name. If it's not able to find it then the command will fail. Alternatively if a file is in the root directory in the format of sp# as in sp1 or sp2 then you may pass a number instead of a file name. If s1 is passed then the device will attempt to open a file name sp1.

- **Sprite File Format**
The bytes in the file must follow the following format.

| Num of Pixels | Num of Frames | R    | G    | B    | ... |
|:--------------|:--------------|:-----|:-----|:-----|-----|
|2 bytes        |2 bytes        |1 byte|1 byte|1 byte| ... |
|[1, 200] = 456 |[0,5] = 5      |255   |2     |0     | ... |

- The sprite file is converted into a sprite object stored in memory. Therefore, it has a size limitation of NumOfFrames * PixelsPerStrip(From the user variable, not the sprite file) * 3(For RGB) < 30K.

- **UDP Polling**
The device will respond to udp packets with bytes in this format

| 1-20 | 21 | data 22+|
|---|---|---|
|EnviralDesignPxlNode| OPCODE | ...|

| OPCODE | Definition | data[bytes] |
|---|---|---|
| 0-99 | ChunkID: Tells the device what index of the pixel bus object to start applying the data to | RGB values[3]... |
| 100 | UpdateFrame: Tells the device to show the data stored in the pixel bus object| No Data |
| 101-199 | Reserved | Reserved |
| 200 | Poll: Pings device to get a formatted response | No Data |
| 201 | PollReply: Response from the device after a ping | DeviceName[64] PixelsPerStrip[2] ChunkSize[2] UdpPort[2] AmpLimitFloat[4] mAPerPixel[2] WarmUpColor[3] |
| 202 | Config: Configures the device with the following format | DeviceName[64] PixelsPerStrip[2] ChunkSize[2] UdpPort[2] AmpLimitFloat[4] mAPerPixel[2] WarmUpColor[3] |
