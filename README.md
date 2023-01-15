Trucki2Shelly/Tasmota/MQTT Gateway (T2SG) V1.05
-----------------------------------------------
![Overview T2SG](./assets/images/shelly_overview.PNG)
  
This Arduino code was written for a ESP8266 WEMOS D1 mini. Its purpose is to read the total
power from a Shelly 3EM and send it via UART to Trucki's RS485 interface pcb for 
SUN GTIL2-1/2000 MPPT inverter (https://github.com/trucki-eu/RS485-Interface-for-Sun-GTIL2-1000). 
J1-J5 have to be open on the RS485 interface pcb (UART, ID:1). On newer RS485 interface pcbs J5 is a switch.
  
The total power is filterd (ZeroExportController) before it is send to the inverter. The goal
of the ZeroExportController is to keep the Shelly total power at ~50W. The inverter power 
rises slowly over ~30s and drops fast (~1s). 
  
The T2SG starts flashing blue every ~500ms if it is connected to your wifi and ready to use.

As I do not own a Shelly 3EM (only a 1PM) I tested this code with a tasmota and a SDM630         smartmeter in a productive environment.
  
WEMOS Adapter
-------------
I sell ( RS485(a)trucki(point)eu ) a WEMOS Adapter for my pcb as well. With this adapter (WEMOS not included) 
you can mount a WEMOS D1 mini with external antenna into the SUN GTIL2 1/2000 inverter:
  
<img src="./assets/images/WEMOS Adapter Set_small.jpg" width="200">

<img src="/assets/images/SUN1000 RS485 pcb WEMOS D1 mini Pro_small.jpg" width="200">
  
<img src="/assets/images/SUN1000_ext.Antenne_small.jpg" width="200">  
   
There are some WEMOS/ESP8266 modules with a high power consumption on startup. This might cause problems during the boot process of the ESP8266 processor. To prevent this you can mount a 220uF electrolyte capacitor to the VCC/GND pin of the RS485 board. Make sure that the polarity is right:
  
<img src="/assets/images/220uF_25V.JPG" width="200"> 
  
I'll integrate this capacitors to newer pcb versions (28.11.2022) . The production date is marked on the pcb. 
    
Setup
-----
After the first start (~30s) you will find an open accesspoint named:"Trucki2Shelly Gateway".
Connect to it and open http://192.168.4.1 in your browser:

<img src="/assets/images/Wifimanager.png" width="200">

Select Configuration and set your wifi credentials, static IP, Subnet,Gateway address for this ESP8266 Wemos module. DHCP is not supported anymore.
  
The Shelly IP address, the maximum power of the solar inverter and mqtt  are no longer configured with the WifiManager. Use the Settings link on the WebServer to configure these things. 
  
Once you have finished you can reset the module and check if it connects to your wifi. 
  
Webserver
---------
Once your ESP8266 Wemos module is set up you can open the configured IP adress in your browser.
You will see a very basic webserver with all values and a Settings link at the end of the page.
The website will autoreload every 2s.

<img src="/assets/images/WebServer.JPG" width="200">

On the Settings-Page you can configure the following settings:
  
Shelly_url: 

(default: http://192.168.1.217/status  for Shelly 3EM) ; url where the T2SG can find a json 
structure with the current grid power.

json keys:

(default: total_power  for Shelly 3EM) ; json key for grid power in the received json structure.
Use < meters,0,power > for a Shelly 1PM.
 
shelly intervall[ms]:

(default: 500) grid power will be captured via http request every x ms.
   
Max power[W]:

(default: 850) maximum inverter power. Use max. 850/1850W for SUN 1000 / 2000
   
ZEPC target[W]:

(default 25-75) Target for ZeroExportController. The ZEPC controls the inverter power to reach a 
grid power in the defined range.
   
ZEPC average:

(default 60) ZeroExportController calculates inverter power over average=60 Shelly values. If 
Shelly power is lower that grid_target a new value is calculated instandly.
     
mqtt server: (default: 192.168.1.225) mqtt broker ip
mqtt port: (default: 1883) mqtt broker port
mqtt username: (default: mqtt_user)
mqtt password: (default: mqtt_pass)
mqtt device name:  (default: T2SG) Use different mqtt names if you have several T2SGs.

<img src="/assets/images/Settings.JPG" width="200">
 
Tasmota instead Shelly
-----------------------
<img src="/assets/images/Tasmota_IR_Head.JPG" width="400">
    
Tasmota's SML scripting tool is very powerful and supports many smartmeters (IR,Modbus,...)
  
https://tasmota.github.io/docs/Smart-Meter-Interface/#meter-definition
  
By changing shelly_url and json_keys of the Trucki2Shelly Gateway you can import Tasmota 
grid power (shelly_power) for the ZeroExportController from your Tasmota. 
The shelly_url for your Tasmota should look like this:
  
http://ip-address/cm?cmnd=status%2010 . 
  
The json_keys are dependend on your Tasmota script configuration. 
The first json key is StatusSNS . If your Tasmota SML script i.e. looks like this:
```
>D
>B
=>sensor53 r
tper=20
>M 1
+1,13,o,0,300,SML,15,32,2F3F210D0A,063030300D0A
1,1.8.1*00(@1),Power_curr,W,Power_curr,2
```
Your second json_key is SML and your third json_key is Power_curr .
So, on the Trucki2Shelly Configuration page you have to enter for the
json_keys: StatusSNS,SML,Power_curr    
 
Serial debug interface
----------------------
If you open a Termianl monitor with 9600baud, 8N1 you will get debug information during the
start.

Connect your WEMOS module via USB to your Windows computer and open your Windows Device Manager 
to find out the COM-Port number. You might need to install additional USB Drivers for the CP2104
USB Bridge of the WEMOS module (https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads)

<img src="/assets/images/DeviceManager.png" width="200">

Download, install and start i.e. Putty from https://putty.org .
Select "Serial" as connection type and correct the COM-Port number:

<img src="/assets/images/Putty_Settings.JPG" width="400">

Press "OPEN" and RESET your WEMOS module. You'll get the Debug output of your module:

<img src="/assets/images/Putty_output.JPG" width="400">
  
MQTT client publish (read only)
-------------------------------
The mqtt client publishes every 1.3s the following topics: 
```
T2SG/ACSetpoint
T2SG/ACDisplay [W]
T2SG/VGrid [V]
T2SG/VBat [V]
T2SG/DAC
T2SG/Calstep
T2SG/Temperature [°C]
T2SG/Shellypower [W]
```
T2SG is the mqtt device name which can be changed in the settings. MQTT Name and topics are case sensitive. If you have several T2SG's make sure that each T2SG has its own name.
  
MQTT client subscribe (write only)
---------------------------------- 
By writing to the mqtt topics: 
```
T2SG/ACSetpointOVR [W] 
T2SG/CalstepOVR [1:start calib., 99:default LUT]
T2SG/DACOVR [0-65535]
```
You can overwrite the current values. The ZeroExportController will be disabled.
The ZeroExportController will be enabled again if you write to T2SG/ShellypowerOVR [W].
You can use T2SG/ShellypowerOVR to feed the ZeroExportController instead of the Shelly.
Please make sure that the shelly_url ist blank. Otherwise you will have two 
devices (Shelly and MQTT) trying to feed the ZeroExportController. If you use 
Shelly_power override mqtt value make sure that your meter is connected between grid and 
inverter. Means if you increase the inverter power your meter value (grid power) will decrease.
If you just want to control the output power of the inverter use ACSetpoint override.

Calibration of a Trucki RS485 pcb:
----------------------------------
A calibration of a Trucki RS485 pcb is only necessary if the difference between AC_Setpoint
and AC_Display is more than ~15%. Make sure that your DC Source (i.E. battery) has enough 
power to power your inverter for at least 1min at maximum power. To start the calibration
go to the settings-page (!) and enter the following address in your browser:
http://T2SG-IP-address/calibration?step=1 
If you want to reload the default calibration enter this address in your browser:
http://T2SG-IP-address/calibration?step=99   

HomeAssistant mqtt read/write
-----------------------------
there are better websites to learn how to connect HomeAssistant with a mqtt device.
Just two screenshots. To get this page in HomeAssistant:
  
<img src="/assets/images/T2SG_HomeAssistant.jpg" width="200">

you will have to add a configuation like this to your configuration.yaml:
 
<img src="/assets/images/T2SG_HomeAssistant_MQTT_Config.JPG " width="200">
  
Flashing
--------
I use "ESP.Easy.Flasher.exe" to flash the bin-file to the WEMOS module. 
  
https://github.com/Grovkillen/ESP_Easy_Flasher/releases
  
Before starting you have to copy the bin-file into the /bin folder. After starting select USB Port, bin-file and baudrate = 115200, press flash. After a reset you can begin with the SETUP.
  
<img src="/assets/images/ESPEasyFlasher.JPG" width="200">
  
The newest bin file is here: https://github.com/trucki-eu/Trucki2Shelly-Gateway/blob/main/bin/
 
Config Reset
------------
If you want to reset the configuration and go back to the "Trucki2Shelly Gateway" accesspoint
you can connect & hold D0 to D5 (i.e. with a screwdriver) and press the reset button on your 
WEMOS module. 
  
<img src="/assets/images/T2SG_Reset.JPG" width="200">
  
The serial debug interface will show: "CONFIG RESET" and after ~30s you can 
connect to the accesspoint.

The blue LED will be constant ON if the config reset was successful.

Blue LED
---------
The blue LED on the WEMOS board flashes every ~1s if connected to your wifi. If the WEMOS module is not configured (Accesspoint "Trucki2Shelly Gateway" is active) the blue LED is constant ON.

Peculiarities WEMOS D1 mini PRO
-------------------------------
I had some WEMOS D1 mini Pro modules crashing during boot or normal operation. I discoverd voltage drops on the internal 3.3V line of the WEMOS Pro modules:

<img src="/assets/images/3_3V_voltage_drop.JPG" width="400">

Adding a 10uF capacitor between 3.3V and GND helped:

<img src="/assets/images/WEMOS PRO+10uF.JPG" width="400">

  
Compiling
---------
You don't need to compile this code. Just download the bin file from the bin folder of this project and flash it to your ESP8266 WEMOS module.
If you want to compile it anyway the source code is here: https://github.com/trucki-eu/Trucki2Shelly-Gateway/blob/main/src/
I used the standard Arduino IDE 1.8.13 with ESP SDK3.0.2 to compile this code. Generic ESP8266, Flash Size 1MB (FS:64KB, OTA:~470KB) .
