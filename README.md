# esp-idf-smtp-camera
Take a picture and Publish it via SMTP.   

![スライド1](https://user-images.githubusercontent.com/6020549/99764383-47f3e680-2b40-11eb-89ea-152d1b9c6aac.JPG)

![スライド2](https://user-images.githubusercontent.com/6020549/99764391-49bdaa00-2b40-11eb-9ebf-8b5b9b815127.JPG)

# Software requirements
esp-idf v4.0.2-120.   
git clone -b release/v4.0 --recursive https://github.com/espressif/esp-idf.git

esp-idf v4.1-520.   
git clone -b release/v4.1 --recursive https://github.com/espressif/esp-idf.git

esp-idf v4.2-beta1-227.   
git clone -b release/v4.2 --recursive https://github.com/espressif/esp-idf.git

__It does not work with esp-idf v4.3.__   
__Even if I fix [this](https://github.com/espressif/esp-idf/pull/6029), I still get a panic.__

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
git clone https://github.com/espressif/esp32-camera components
make menuconfig
make flash monitor
```

# Start firmware
Change GPIO0 to open and press the RESET button.

# Configuration
Set the following items using menuconfig.

![config-main](https://user-images.githubusercontent.com/6020549/66692052-c17e9b80-ecd5-11e9-8316-075350ceb2e9.jpg)

![config-app](https://user-images.githubusercontent.com/6020549/100705148-0f7ac500-33ea-11eb-8497-6bede2901a72.jpg)

## Wifi Setting

![config-wifi-1](https://user-images.githubusercontent.com/6020549/100705156-11448880-33ea-11eb-9de9-89f05744a6d4.jpg)

You can use static IP.   

![config-wifi-2](https://user-images.githubusercontent.com/6020549/100705166-16a1d300-33ea-11eb-986d-4ff228e72d97.jpg)

## SMTP Server Setting

![config-smtp](https://user-images.githubusercontent.com/6020549/98746220-64817780-23f8-11eb-99c1-f5a4dde2e1e8.jpg)

__I only tested gmail accounts.__

## Attached File Name Setting

Select the attached file name from the following.   
- Always the same file name   
- File name based on date and time   
When you choose date and time file name, you will need an NTP server.   
The file name will be YYYYMMDD-hhmmss.jpg.   

![config-filename-1](https://user-images.githubusercontent.com/6020549/98748023-1d958100-23fc-11eb-8bc9-9b65306be2a3.jpg)
![config-filename-2](https://user-images.githubusercontent.com/6020549/98748025-1ec6ae00-23fc-11eb-9770-e00618b4097c.jpg)

## Camera Pin

![config-camerapin](https://user-images.githubusercontent.com/6020549/66692087-1d492480-ecd6-11e9-8b69-68191005a453.jpg)

## Picture Size

![config-picturesize](https://user-images.githubusercontent.com/6020549/66692095-26d28c80-ecd6-11e9-933e-ab0be911ecd2.jpg)

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
You can connect with mDNS hostname.   
You can use tcp_send.py.   
```
python ./tcp_send.py
```

![config-shutter-3](https://user-images.githubusercontent.com/6020549/100706730-d2fc9880-33ec-11eb-80da-80cc8278ae43.jpg)

- Shutter is UDP Socket   
You can use udp_send.py.   
```
python ./udp_send.py
```

![config-shutter-4](https://user-images.githubusercontent.com/6020549/100706733-d2fc9880-33ec-11eb-85d2-62b988720d75.jpg)

## Flash Light

ESP32-CAM by AI-Thinker have flash light on GPIO4.

![config-flash](https://user-images.githubusercontent.com/6020549/98637034-46216a80-236b-11eb-8504-e83f718f5e85.jpg)

