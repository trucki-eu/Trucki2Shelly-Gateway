 Trucki2Shelly Gateway (T2SG)
  ---------------------------
  ![Overview T2SG](/assets/images/shelly_overview.PNG)
  
  This Arduino code was written for a ESP8266 WEMOS D1 mini. Its purpose is to read the total
  power from a Shelly 3EM and send it via UART to Trucki's RS485 interface pcb for 
  SUN GTIL2-1/2000 MPPT inverter (https://github.com/trucki-eu/RS485-Interface-for-Sun-GTIL2-1000). 
  J1-J5 have to be open on the RS485 interface pcb (UART, ID:1).
  
  The total power is filterd (ZeroExportController) before it is send to the inverter. The goal
  of the ZeroExportController is to keep the Shelly total power at ~50W. The inverter power 
  rises slowly over ~30s and drops fast (~1s). 
  
  As I do not own a Shelly 3EM (only a 1PM) I wasn't able to test this code in a productive 
  environment.
  
  Setup
  -----
  After the first start (~30s) you will find an open accesspoint named:"Trucki2Shelly Gateway".
  Connect to it and open http://192.168.4.1 in your browser:
  
  <img src="/assets/images/Wifimanager.png" width="200">
   
  Select Configuration and set your wifi credentials, static IP, Subnet,Gateway address for this 
  ESP8266 Wemos module, the Shelly IP address, the maximum power of the solar inverter and 
  mqtt server, user and password if you want to publish mqtt data. Use 0.0.0.0 for static IP, 
  subnet and getway if you want to get an IP address from your DHCP server.
 
  <img src="/assets/images/Wifimanager_config.png" width="600">
  
  Once you have finished you can reset the module and check if it connects to your wifi. 
  
  Serial debug interface
  ----------------------
  If you open the Arduino Serial monitor with 9600 you will get debug information during the 
  start:
  
  <img src="/assets/images/wifi_debug_output.JPG" width="400">
  
  Webserver
  ---------
  Enter the static IP address of the module in your browser and you will get a very basic 
  webserver with all values. The website will autoreload every 2s.
  
  <img src="/assets/images/T2SG_Webserver.png" width="200">
  
  MQTT client
  -----------
  The mqtt client publishes every 5s the following topics: 
  T2SG/ACSetpoint, T2SG/ACDisplay, T2SG/VGrid, T2SG/VBat, T2SG/DAC, T2SG/Calstep, T2SG/Temperature, T2SG/Shellypower.
  You can use the MQTT client to log data i.e. with HomeAssistant.
  
  Flashing
  --------
  I use "ESP.Easy.Flasher.exe" to flash the bin-file to the WEMOS module. Before starting you 
  have to copy the bin-file into the /bin folder. After starting select USB Port and bin-file 
  and press flash. After a reset you can begin with the setup.
  
  <img src="/assets/images/ESPEasyFlasher.JPG" width="200">
  
  The bin file is here: https://github.com/trucki-eu/Trucki2Shelly-Gateway/blob/main/bin/Trucki2Shelly_Gateway.ino.bin
 
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
  You don't need to compile this code. Just download the bin file and flash it to your ESP8266 WEMOS module.
  If you want to compile it anyway the source code is here: https://github.com/trucki-eu/Trucki2Shelly-Gateway/blob/main/src/Trucki2Shelly_Gateway.ino
  I used the standard Arduino IDE 1.8.13 with ESP SDK3.0.2 to compile this code. 
