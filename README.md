# esp-idf-smtp-camera
Take a picture and Publish it via SMTP.   
This project use [ESP32 Camera Driver](https://github.com/espressif/esp32-camera).   

![slide0001](https://user-images.githubusercontent.com/6020549/122657146-caf32300-d19b-11eb-95c0-d053b77599c7.jpg)
![slide0002](https://user-images.githubusercontent.com/6020549/122657145-c9c1f600-d19b-11eb-9049-17dc2d71acdc.jpg)

# Software requirements
esp-idf v4.3 or later.   

# Installation
Use a USB-TTL converter.   

|ESP-32|USB-TTL|
|:-:|:-:|
|U0TXD|RXD|
|U0RXD|TXD|
|GPIO0|GND|
|5V|5V|
|GND|GND|


```
git clone https://github.com/nopnop2002/esp-idf-smtp-camera
cd esp-idf-smtp-camera
git clone https://github.com/espressif/esp32-camera components/esp32-camera
chmod 777 getpem.sh
./getpem.sh
idf.py set-target esp32
idf.py menuconfig
idf.py flash monitor
```

# Start firmware
Change GPIO0 to open and press the RESET button.

# Configuration
Set the following items using menuconfig.

![config-main](https://user-images.githubusercontent.com/6020549/121621472-30ae2380-caa7-11eb-8428-1dd5273bd668.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/121621482-33107d80-caa7-11eb-8c29-ecb9ad6f3912.jpg)

## Wifi Setting

![config-wifi-1](https://user-images.githubusercontent.com/6020549/121621478-31df5080-caa7-11eb-9ec4-90fc8375cbdb.jpg)

You can use the mDNS hostname instead of the IP address.   
- esp-idf V4.3 or earlier   
 You will need to manually change the mDNS strict mode according to [this](https://github.com/espressif/esp-idf/issues/6190) instruction.   
- esp-idf V4.4 or later  
 If you set CONFIG_MDNS_STRICT_MODE = y in sdkconfig.default, the firmware will be built with MDNS_STRICT_MODE = 1.

![config-wifi-2](https://user-images.githubusercontent.com/6020549/121621479-3277e700-caa7-11eb-9afe-27f1a5091c4d.jpg)

You can use static IP.   
![config-wifi-3](https://user-images.githubusercontent.com/6020549/121621480-3277e700-caa7-11eb-9280-bd66214bf6a7.jpg)


## SMTP Server Setting

![config-smtp](https://user-images.githubusercontent.com/6020549/98746220-64817780-23f8-11eb-99c1-f5a4dde2e1e8.jpg)

__I've only tested with a Gmail account.__   
If you want to use a non-gmail account, you need to change gmail_root_cert.pem according to [this](https://github.com/espressif/esp-idf/issues/7250) instruction.   

Note about Gmail:   
To help keep your account secure, from May 30, 2022, Google no longer supports the use of third-party apps or devices which ask you to sign in to your Google Account using only your username and password.   
You need to generate special app password according [this](https://stackoverflow.com/questions/72577189/gmail-smtp-server-stopped-working-as-it-no-longer-support-less-secure-apps/72626684#72626684).   


## Attached File Name Setting

You can select the file name to write to the shared folder from the following.   
- Always the same file name   
- File name based on date and time   
When you choose date and time file name, you will need an NTP server.   
The file name will be YYYYMMDD-hhmmss.jpg.   

![config-filename-1](https://user-images.githubusercontent.com/6020549/119243498-5203aa00-bba2-11eb-87d5-053636dbb85a.jpg)
![config-filename-2](https://user-images.githubusercontent.com/6020549/119243499-5203aa00-bba2-11eb-8c0f-6bb42d125d64.jpg)

- Add FrameSize to Remote file Name   
When this is enabled, FrameSize is added to remote file name like this.   
`20210520-165740_800x600.jpg`   

![config-filename-3](https://user-images.githubusercontent.com/6020549/119243501-529c4080-bba2-11eb-8ba4-85cdd764b0fc.jpg)

## Select Frame Size
Large frame sizes take longer to take a picture.   

![config-framesize-1](https://user-images.githubusercontent.com/6020549/122479721-7ff3d700-d006-11eb-9910-8fd132f7479b.jpg)
![config-framesize-2](https://user-images.githubusercontent.com/6020549/122479725-81bd9a80-d006-11eb-847b-0748bfba75df.jpg)

## Select Shutter

You can choose one of the following shutter methods

- Shutter is the Enter key on the keyboard   
For operation check

![config-shutter-1](https://user-images.githubusercontent.com/6020549/100706728-d132d500-33ec-11eb-96e2-22d30b2131f5.jpg)

- Shutter is a GPIO toggle

  - Initial Sate is PULLDOWN   
The shutter is prepared when it is turned from OFF to ON, and a picture is taken when it is turned from ON to OFF.   

  - Initial Sate is PULLUP   
The shutter is prepared when it is turned from ON to OFF, and a picture is taken when it is turned from OFF to ON.   

I confirmed that the following GPIO can be used.   

|GPIO|PullDown|PullUp|
|:-:|:-:|:-:|
|GPIO12|OK|NG|
|GPIO13|OK|OK|
|GPIO14|OK|OK|
|GPIO15|OK|OK|
|GPIO16|NG|NG|

![config-shutter-2](https://user-images.githubusercontent.com/6020549/100706729-d2640200-33ec-11eb-8ac5-68abad4d1a0b.jpg)

- Shutter is TCP Socket   
You can use tcp_send.py.   
`python ./tcp_send.py`

![config-shutter-3](https://user-images.githubusercontent.com/6020549/100706730-d2fc9880-33ec-11eb-80da-80cc8278ae43.jpg)

- Shutter is UDP Socket   
You can use udp_send.py.   
`python ./udp_send.py`

![config-shutter-4](https://user-images.githubusercontent.com/6020549/100706733-d2fc9880-33ec-11eb-85d2-62b988720d75.jpg)

## Flash Light

ESP32-CAM by AI-Thinker have flash light on GPIO4.   

![config-flash](https://user-images.githubusercontent.com/6020549/122479750-8f732000-d006-11eb-9e50-16c91cff1bb3.jpg)

