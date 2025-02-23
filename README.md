# Infrared Thermal Imager
<p>
This projects aims to build a device that generates images of the internal part of the human body (to be specific - the neck) using an array of thermal sensors, these images will be processed by an
AI algorithm for serveral kinds of analysis, with the specific use case of detecting thyriod growth in the neck region. Beyond this purpose, the device can be used in a wide range of applications.<br>
<p>


## Contents
1. [Introduction](#introduction)
2. [Device specs and How to use](#device-specs-and-how-to-use)
3. [Replicate this project](#replicate-this-project)
4. [Components Used: Hardware](#components-used-hardware)
5. [ESP32 Setup](#esp32-setup)
6. [Thermal Sensor Setup](#thermal-sensor-setup)
7. [Display Setup](#display-setup)
8. [Switches and others](#switches-and-others)
9. [Contribute to this project](#contribute-to-this-project)

---

### Introduction
The project is divided into 3 main parts:
1. The hardware
2. The firmware
3. The AI model

Only the hardware and firmware are hosted inn this repo, for the ai model please refer to [this repo](https://github.com/PreciousJac0b/Medical-Image-Processing)<br>

The hardware is built using the ESP32 microcontroller, a thermal sensor array, a display and a few switches. The firmware is written in C++ using the Arduino IDE. and at the time of writing this there are two realeases. refer to the [CHANGELOG](/CHANGELOG.md) file for features specific to each release. Further down this file, you will see information about every component used and how to set them up. Finally, the code is self-documenting and the functions are also well documented using C99 style, so if you wish to learn one or two, you can read the code.<br>

The Variant-B branch holds high-speed high-efficiancy code but does not follow standard, as I had to manually download the display library and tweak some functions, accessing registers directly to get the speed I needed. The master branch is the standard code, it is slower but it is easily scalable and follows the standard. The master branch is recommended for beginners and for those who wish to learn from the code.<br>
> [!CAUTION]
> The variabt-B branch is not stable and still under development.

If you care about how it works...<br>
---
Once the device is on, it obtains the temperature value of all the 64 temperature sensors in the thermal sensors (the amg8833 sensor was used and it has 64 pixels), this 8x8 array of temperature value is then interpolated through a data-loss resistive algorithm into a 24x24 array of temperature value. This array then undergo a mapping process where each value is converted to a color (we used RED for high temperature and BLUE for low temperatures. Although the HIGH and LOW can be changed in the header file before compiling) and this colored 24x24 array is then drawn on the 170x220 resolution screen, but how? well the [variant-B](https://github.com/VicTheM/infrared-thermal-imager/tree/variant-B) branch writes to the screen pixel by pixel so a fluid high resolution image is displayed but in this main branch we used a square to represent a pixel, and by this we can draw the 24x24 image on the 176x220 display.

***The Math***

If a rectangle represents a pixel, the height of the rectangle will be the vertical scale factor and the width will be the horizontal scale factor.<br>
Vertical scale factor = screen total vertical pixels / image total vertical pixels<br>
=> 220 / 24 = 9.167<br>
Horizontal scale factor = screen total horizontal pixels / image total horizontal pixels<br>
=> 170 / 24 = 7.083\
As you might already know, writing to a display takes time so scaling using this method help speed up the image rendering process but has tge drawback of desplaying "square-like" low resolution images. the total number of rectangles is the number of writes to be made, and this is the problem that was solved in the variant-B branch. in that branch I manipulated the [display library](https://github.com/VicTheM/infrared-thermal-imager/blob/variant-B/thermal_imager/screen_library.cpp) for a batch write process (they don't have the functionality for colored batch write yet). so we send the command once and they rest will be carried out on the image controller instead of writing every single pixel one at a time.<br>
As I was saying, the process of obtaining the temperature values and displaying them continues indefinitely until either the cpature button is pressed or the server switch is turned on.

---

***Capture button is clicked***  

If the capture button is clicked, the whole process is paused and the 24x24 temperature array is yet interpolatd using an algorithm for fast 1D interpolation into a 170x220 array of temperature value, then this array is converted into RGB565 color valures and these values are used to create a bitmap file from scratch, which is saved in the local memory of esp32. Files are saved sequentially starting from *img_1.bmp* through *img_1000.bmp* before a wrap around. this sequential naming is made possible by a variable that resides in the Non-volatial sorage and keeps count of the files.<br>

Before a file is created, the storage is checked to be sure there is enough space, if there is nospace, all the files in memory will be cleared then the new file saved. The bmp file size is 113KB and it uses 24-bit color depth, with uncompressed data.

> [!WARNING]
> If more than 10 images are on the device they will all be automatically deleted to store new images

---

***Server switch turned on***  

Once the server switch is on, the screen stops displaying images and the capture button is disabled, you can now see instructions on the screen for connecting to the device
The webserber made available has an api to monitor system performance: ```http://{p-addr}:/api/stats``` and a page to view all files: ```http://{ip-adr}/```<br>
In the homepage, you can see all files and a button to either download them or delete them, anyone you do, the server will obey.

When ther server switch is turned off then step one continues - generating the heatmap and displaying on screen.

---

### Device specs and How to use

##### Specs
Version 1.0.0 of the device has the following specs:
- 8x8 thermal sensor array
- 2.0 inch TFT display
- 3 switches
- 2 LED
- 1 Buzzer
- 1 USB-C port
- 3.7V 9000mAh Li-ion battery (overkill)
- Image resolution: 170x240 pixels
- Screen resolution: 170x240 pixels
- Display resollution: scaled 24x24 pixels to 170x240 pixels
- Display refresh rate: 5Hz
- File download format: .bmp
- Image processing time: 3 seconds

##### Features
- Video stream to screen at 5Hz refresh rate
- High quality Image capture
- Download image wirelessly via any browser
- Automatic memory magament ( > v1.0.0 only)
- Low power consumption
- No wifi needed

##### How to use
1. Turn on the device
2. Wait for the device to boot up (3 seconds)
3. video stream the enviroment (whose heatmap would have been showing on the screen)
4. Position thermal camera against the object to capture
5. Click the capture button once (Note: not 'press', click!)
6. You can capture as many images as you want by clicking as many times as needed

> [!NOTE]
> A max of 10 high quality images can be saved locally on the device, capturing more than 10 images at a time will lead to loss of the previous 10 images, so try to download the images before capturing again.

7. Toggle the web server switch to turn it on
8. Follow the instruction on the screen to connect to device and download image
9. Toggle server switch back to start capturing again

### Replicate this project
Want to build this yourself? simple! make a 3d design looking better than mine and put all these components inside, compile any version of the code, load it up and you are good! Below is a more detailed explanation of how to assemble the device, it can also be bread-boarded<br>

### Components used and hardware
1. Microcontroller: ESP32 Wroom-32D
2. Thermal sensor: AMG8833
3. Display: ILI9225 170X220 TFT RGB Display
4. Button switch
5. Toggle switch
6. Buzzer and LED
7. Battery: 3.7v 3000mAh x 3 Li-ion battery
8. Charger: 5v charger module

##### ESP32 setup
---

The [ESP32 DvekitC](https://randomnerdtutorials.com/getting-started-with-esp32/) is very common, and that was what I used for < v1.1.0 of this project. it has 38 pins and I used a number of them. refre to the [Circuit diagram]() for a fine description of the connections. You might have to run the example sketch from the LittleFS library once before loading this program unto the esp32 (the example sketch is used to format the memory as required).
You can find a tutorial about this Esp32 [Here](https://randomnerdtutorials.com/getting-started-with-esp32/) and the Arduino-esp32 software documentation [Here](https://docs.espressif.com/projects/arduino-esp32/en/latest/)

##### Thermal sensor setup
---

The AMG8833 is a versatile low resolution thermal sensor that has 64 temperature sensors and return all their values as an array, so in our program, we interpolated that 8x8 array to a 170x220 arry to get the 170x220 bit image

---

***Pin connections: AMG8833 - ESP32***<br>
_vcc --- 3.3v_<br>
_GND --- GND_<br>
_SDA --- D21_<br>
_SCL --- D22_<br>
You can find a tutorial about this sensor [Here]() and the library documentation [Here]()

---

##### Display setup
The 2.0 inches RGB ILI9225 TFT Display was used in my case, while you can use other displays, it is strongly advised to use this one since they library is not portable and would only work for screens driven by the ILI9225 chip. The display has a resolution of 170x220 pixels and is driven by the ILI9225 chip. The display is connected to the ESP32 using the SPI protocol, although there are options to use the I2C protocol, the SPI protocol is faster and more reliable.<br>

---
***Pin connections: ILI9225 Display - ESP32***<br>
_VCC --- 5V_<br>
_GND --- GND_<br>
_LED --- 5V_<br>
_CLK --- D14_<br>
_SDI --- D13_<br>
_RS --- D25_<br>
_RST --- D26_<br>
_CS --- D15_<br>

You can find a tutorial about this display [Here](https://www.hackster.io/jdanielse/amg8833-thermal-camera-fc8478) and the library documentation [Here](https://github.com/Nkawu/TFT_22_ILI9225/wiki)

---

##### Switches and others
Remember we used switches to control states and led as indicator, so below is a brief description of how to connect them

---
***Pin connections: Switches and LED - ESP32***
_Capture button left leg --- D19_<br>
_Capture button right leg --- GND_ (with a 5.7k pull-down resistor)<br>
_Server switch left leg --- D18_<br>
_Server switch right leg --- GND_ (with a 5.7k pull-down resistor)<br>
_LED --- D12_ (with a 220 ohm resistor)<br>

### Contribute to this project
There are a number of ways you can contribute to this project, you can:
1. Report bugs
2. Suggest new features
3. Write code
4. Improve documentation
5. Make a new 3D design

> [!TIP]
> You can reach me on [X](https://twitter.com/Victory2702) or via email at [victorychibuike111@gmail.com](mailto:victorychibuike111@gmail.com)
