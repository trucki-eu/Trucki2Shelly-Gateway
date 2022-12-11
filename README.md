 Trucki2Shelly/Tasmota/MQTT Gateway  (T2SG) V1.03
  -----------------------------------------------
  ![Overview T2SG](/assets/images/shelly_overview.PNG)
  
  This Arduino code was written for a ESP8266 WEMOS D1 mini. Its purpose is to read the total
  power from a Shelly 3EM and send it via UART to Trucki's RS485 interface pcb for 
  SUN GTIL2-1/2000 MPPT inverter (https://github.com/trucki-eu/RS485-Interface-for-Sun-GTIL2-1000). 
  J1-J5 have to be open on the RS485 interface pcb (UART, ID:1). On newer RS485 interface pcbs J5 is a switch.
  
  The total power is filterd (ZeroExportController) before it is send to the inverter. The goal
  of the ZeroExportController is to keep the Shelly total power at ~50W. The inverter power 
  rises slowly over ~30s and drops fast (~1s). 
  
  As I do not own a Shelly 3EM (only a 1PM) I wasn't able to test this code in a productive 
  environment.
  
  WEMOS Adapter
  -------------
  I sell ( RS485(a)trucki(point)eu ) a WEMOS Adapter for my pcb as well. With this adapter (WEMOS not included) 
  you can mount a WEMOS D1 mini with external antenna into the SUN GTIL2 1/2000 inverter:
  
  <img src="/assets/images/WEMOS Adapter Set_small.jpg" width="200">

  <img src="/assets/images/SUN1000 RS485 pcb WEMOS D1 mini Pro_small.jpg" width="200">
  
  <img src="/assets/images/SUN1000_ext.Antenne_small.jpg" width="200">  
   
  There are some WEMOS/ESP8266 modules with a high power consumption on startup. This might cause problems 
  during the boot process of the ESP8266 processor. To prevent this you can mount a 220uF electrolyte 
  capacitor to the VCC/GND pin of the RS485 board. Make sure that the polarity is right:
  
  <img src="/assets/images/220uF_25V.JPG" width="200"> 
  
  I'll integrate this capacitors to newer pcb versions (28.11.2022) . The production date is marked on the pcb. 
  
  
  Setup
  -----
  After the first start (~30s) you will find an open accesspoint named:"Trucki2Shelly Gateway".
  Connect to it and open http://192.168.4.1 in your browser:
  
  <img src="/assets/images/Wifimanager.png" width="200">
   
  Select Configuration and set your
  wifi credentials, static IP, Subnet,Gateway address for this ESP8266 Wemos module, the Shelly
  IP address, the maximum power of the solar inverter and mqtt server, user and password if you 
  want to publish mqtt data.Use json_keys: < total_power > for a Shelly 3EM and < meters,0,power > for a Shelly 1PM. 
  
  You can even use a Tasmota instead a Shelly by modifing the shelly url (default: http://< shelly-ip >/status) and json_keys.
  Use 0.0.0.0 for static IP, subnet and getway if you want to get an IP address from your DHCP     server.
 
  <img src="/assets/images/V1.02_config_portal.JPG" width="600">
  
  Once you have finished you can reset the module and check if it connects to your wifi. 
  
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
  If you open the Arduino Serial monitor with 9600 you will get debug information during the 
  start:
  
  <img src="/assets/images/wifi_debug_output.JPG" width="400">
  
  Webserver
  ---------
  Enter the static IP address of the module in your browser and you will get a very basic 
  webserver with all values. The website will autoreload every 2s.
  
  <img src="/assets/images/T2SG_Tasmota_SDM630_Website.JPG" width="200">
  
  MQTT client publish (read only)
  -------------------------------
  The mqtt client publishes every 5s the following topics: 
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
  You can use the MQTT client to log data i.e. with HomeAssistant.
  
  MQTT client subscribe (write only)
  ---------------------------------- 
  By writing to the mqtt topics: 
  ```
  T2SG/ACSetpointOVR [W] 
  T2SG/CalstepOVR [1:start calib., 99:default LUT]
  T2SG/DACOVR [0-65535]
  ```
  you can overwrite the current values. The ZeroExportController will be disabled.
  The ZeroExportController will be enabled again if you write to T2SG/ShellypowerOVR [W].
  You can use T2SG/ShellypowerOVR to feed the ZeroExportController instead of the Shelly.
  Please make sure that the shelly_url ist blank. Otherwise you will have two 
  devices (Shelly and MQTT) trying to feed the ZeroExportController. If you use 
  Shelly_power override mqtt value make sure that your meter is connected between grid and 
  inverter. Means if you increase the inverter power your meter value (grid power) will decrease.
  If you just want to control the output power of the inverter use ACSetpoint override.
  
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
  
  Before starting you have to copy the bin-file into the /bin folder. After starting select USB Port and bin-file 
  and press flash. After a reset you can begin with the setup.
  
  <img src="/assets/images/ESPEasyFlasher.JPG" width="200">
  
  The bin file is here: https://github.com/trucki-eu/Trucki2Shelly-Gateway/blob/main/bin/
 
  Config Reset
  ------------
  If you want to reset the configuration and go back to the "Trucki2Shelly Gateway" accesspoint
  you can connect & hold D0 to D5 (i.e. with a screwdriver) and press the reset button on your 
  WEMOS module. 
  
  <img src="/assets/images/T2SG_Reset.JPG" width="200">
  
  The serial debug interface will show: "CONFIG RESET" and after ~30s you can 
  connect to the accesspoint.
  
  Compiling
  ---------
  You don't need to compile this code. Just download the bin file from the bin folder of this project and flash it to your ESP8266 WEMOS module.
  If you want to compile it anyway the source code is here: https://github.com/trucki-eu/Trucki2Shelly-Gateway/blob/main/src/Trucki2Shelly_Gateway_V1.02.ino
  I used the standard Arduino IDE 1.8.13 with ESP SDK3.0.2 to compile this code. Generic ESP8266, Flash Size 1MB (FS:64KB, OTA:~470KB) .
