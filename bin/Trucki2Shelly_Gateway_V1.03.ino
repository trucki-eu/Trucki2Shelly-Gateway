/*
 * Trucki2Shelly/Tasmota/MQTT Gateway V1.03
 * -----------------------------------------
 * This Arduino code was written for a ESP8266 WEMOS D1 mini. Its purpose is to read the total
 * power from a Shelly 3EM and send it via UART to Trucki's RS485 interface pcb for 
 * SUN GTIL2-1/2000 MPPT inverter. J1-J5 have to be open on the RS485 interface pcb (UART, ID:1).
 * On newer RS485 interface pcbs J5 is a switch.
 * 
 * The total power (shelly_power) is filterd (ZeroExportController) before it is send to the 
 * inverter. The target of the ZeroExportController is to keep the Shelly total power at ~50W. 
 * The inverter power rises slowly over ~30s and drops fast (~1s). 
 * 
 * As I do not own a Shelly 3EM (only a 1PM) I wasn't able to test this code in a productive 
 * environment.
 * 
 * Setup
 * -----
 * After the first start (~30s) you will find an open accesspoint named:"Trucki2Shelly Gateway".
 * Connect to it and open http://192.168.4.1 in your browser. Select Configuration and set your
 * wifi credentials, static IP, Subnet,Gateway address for this ESP8266 Wemos module, the Shelly
 * IP address, the maximum power of the solar inverter and mqtt server, user and password if you 
 * want to publish mqtt data.Use json_keys: < total_power > for a Shelly 3EM and < meters,0,power >
 * for a Shelly 1PM. You can even use a Tasmota instead a Shelly by modifing the 
 * shelly url (default: http://< shelly-ip >/status) and json_keys.
 * Use 0.0.0.0 for static IP, subnet and getway if you want to get an
 * IP address from your DHCP server.
 * Once you have finished you can reset the module and check if it connects to your wifi.
 * 
 * Tasmota insted Shelly
 * ---------------------
 * Tasmota's SML scripting tool is very powerful and supports many smartmeters (IR,Modbus,...): 
 * https://tasmota.github.io/docs/Smart-Meter-Interface/#meter-definition
 * By changing shelly_url and json_keys of the Trucki2Shelly Gateway you can import Tasmota 
 * grid power (shelly_power) for the ZeroExportController from your Tasmota. 
 * The shelly_url for your Tasmota should look like this:
 * http://<tasmota-ip>/cm?cmnd=status%2010 . 
 * The json_keys are dependend on your Tasmota script configuration. The first json key is 
 * StatusSNS . If your Tasmota SML script i.e. looks like this:
 * 
 * >D
 * >B
 * =>sensor53 r
 * tper=20
 * >M 1
 * +1,13,o,0,300,SML,15,32,2F3F210D0A,063030300D0A
 * 1,1.8.1*00(@1),Power_curr,W,Power_curr,2
 * 
 * Your second json_key is SML and your third json_key is Power_curr .
 * So, on the Trucki2Shelly Configuration page you have to enter for the
 * json_keys: StatusSNS,SML,Power_curr    
 * 
 * Serial debug interface
 * ----------------------
 * If you open the Arduino Serial monitor with 9600 you will get debug information during the 
 * start.
 * 
 * Webserver
 * ---------
 * Enter the static IP address of the module in your browser and you will get a very basic 
 * webserver with all values. The website will autoreload every 2s.
 * 
 * MQTT client publish (read only)
 * -------------------------------
 * The mqtt client publishes every 1.3s the following topics: T2SG/ACSetpoint, T2SG/ACDisplay,
 * T2SG/VGrid, T2SG/VBat, T2SG/DAC, T2SG/Calstep, T2SG/Temperature, T2SG/Shellypower.
 * You can use the MQTT client featurevto log data i.e. with HomeAssistant.
 * 
 * MQTT client subscribe (write only)
 * ---------------------------------- 
 * By writing to the mqtt topics: T2SG/ACSetpointOVR [W], 
 * T2SG/CalstepOVR [1:start calib., 99:default LUT], T2SG/DACOVR [0-65535], you can overwrite
 * the current values. The ZeroExportController will be disabled.
 * The ZeroExportController will be enabled again if you write to T2SG/ShellypowerOVR [W].
 * You can use T2SG/ShellypowerOVR to feed the ZeroExportController instead of the Shelly.
 * Please make sure that the shelly_url ist blank. Otherwise you will have two 
 * devices (Shelly and MQTT) trying to feed the ZeroExportController. If you use 
 * Shelly_power override mqtt value make sure that your meter is connected between grid and 
 * inverter. Means if you increase the inverter power your meter value (grid power) will decrease.
 * If you just want to control the output power of the inverter use ACSetpoint override.
 * 
 * Flashing
 * --------
 * I use "ESP.Easy.Flasher.exe" to flash the bin-file to the WEMOS module. Before starting you 
 * have to copy the bin-file into the /bin folder. After starting select USB Port and bin-file 
 * and press flash. After a reset you can begin with the setup.
 * 
 * Config Reset
 * ------------
 * If you want to reset the configuration and go back to the "Trucki2Shelly Gateway" accesspoint
 * you can connect & hold D0 to D5 (i.e. with a screwdriver) and press the reset button on your 
 * WEMOS module. The serial debug interface will show: "CONFIG RESET" and after ~30s you can 
 * connect to the accesspoint.
 * 
 * Compiling
 * ---------
 * I used the standard Arduino IDE 1.8.13 with ESP SDK3.0.2 to compile this code.  
 * Generic ESP8266, Flash Size 1MB (FS:64KB, OTA:~470KB)
 * 
 * //------------------------------------------------------------------------------------------- 
 * 1.03 26.11.2022 : Bugfix: IP address was set static to 192.168.1.133 . Now it's config dependend
 * 1.02 21.11.2022 : WifiManager: Variable JSON keys, http-get intervall for Tasmota grid power read
 *                   Bugfix: Save Configuration even if not connected to wifi after config portal
 *                   SPIFFS replaced by eeprom read/write
 *                   MQTT override for ShellyPower, ACSetpoint and DAC
 * 1.01 13.11.2022 : Debug Output for Shelly_read, Version ouput on startup 
 * 1.00 28.10.2022 : 1st version
 * //------------------------------------------------------------------------------------------- 
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ModbusMaster.h>         //https://github.com/4-20ma/ModbusMaster - by DocWalker
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>          //Benoit Blanchon ARDUINOJSON_VERSION_MAJOR >= 6
#include <StreamUtils.h>          //Benoit Blanchon

#define EEPROM_SIZE 1024
#define VERSION "1.02 21.11.2022"

DNSServer dnsServer;
ESP8266WebServer server(80);

WiFiClient espClient, httpClient;
MqttClient mqttClient(espClient);

//MQTT topics
#define MQTT_ACSETPOINT     "T2SG/ACSetpoint"    //r  Sun GTIL2 AC_Setpoint [W]
#define MQTT_ACSETPOINTOVR  "T2SG/ACSetpointOVR" //w  mqtt override ac_setpoint in [W] (int)(ZeroExportController disabled)  
#define MQTT_ACDISPLAY      "T2SG/ACDisplay"     //ro Sun GTIL2 AC_Display output[W]
#define MQTT_VGRID          "T2SG/VGrid"         //ro Sun GTIL2 grid voltage [V]
#define MQTT_VBAT           "T2SG/VBat"          //ro Sun GTIL2 battery voltage[V]
#define MQTT_DAC            "T2SG/DAC"           //ro Sun GTIL2 DAC value
#define MQTT_DACOVR         "T2SG/DACOVR"        //w  mqtt override for Sun GTIL2 DAC value (int) (ACSetpoint/ZeroExportController disabled)
#define MQTT_CALSTEP        "T2SG/Calstep"       //ro Sun GTIL2 calibration step
#define MQTT_CALSTEPOVR     "T2SG/CalstepOVR"    //w  mqtt send 1 to start calibration. Write 99 to load default LUT (int)
#define MQTT_TEMPERATURE    "T2SG/Temperature"   //ro Sun GTIL2 Temperature [°C]
#define MQTT_SHELLYPOWER    "T2SG/Shellypower"   //ro ShellyPower [W] = Grid Power from http get request. Used for ZeroExport Controller
#define MQTT_SHELLYPOWEROVR "T2SG/ShellypowerOVR"//w  mqtt override ShellyPower (uint16). I.e. to send external grid power to ZeroExport Controller (Shelly url musst be empty)

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40]    = "192.168.1.225";     //MQTT Broker IP
char mqtt_port[6]       = "1883";              //MQTT Broker port
char mqtt_user[20]      = "mqtt_user";         //MQTT Broker user
char mqtt_pass[20]      = "mqtt_pass";         //MQTT Broker password
char maxPower[6]        = "850";               //maximum inverter power
char static_ip[16]      = "192.168.1.133";     //Trucki2Shelly IP
char static_gw[16]      = "192.168.1.1";       //Network gateway IP
char static_sn[16]      = "255.255.255.0";     //Network Subnetmask
char shelly_url[100]    = "http://192.168.1.217/status";//for Shelly //"http://192.168.1.141/cm?cmnd=status%2010"; //for Tasmota;
char json_keys[60]      = "total_power";       //< total_power > for Shelly 3EM // < StatusSNS,0,Power_curr > //for Tasmota SML //< meters,0,power >for Shelly 1PM  
char shelly_interval[16]= "500";               //http-get grid power from Shelly/Tasmota interval in [ms]

String mqtt_ACSetpointOVR ="";                 //Payload of last mqtt message with topic T2SG/ACSetpointOVR 
String mqtt_DACOVR ="";                        //Payload of last mqtt message with topic T2SG/DACOVR 
String mqtt_CALSTEPOVR  ="";                   //Payload of last mqtt message with topic T2SG/CalstepOVR 
String mqtt_SHELLYPOWEROVR ="";                //Payload of last mqtt message with topic T2SG/ShellypowerOVR 

WiFiManagerParameter custom_mqtt_server    ("server"   ,       "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port      ("port"     ,       "mqtt port"  , mqtt_port  , 6);
WiFiManagerParameter custom_mqtt_user      ("user"     ,       "mqtt user"  , mqtt_user  , 20);
WiFiManagerParameter custom_mqtt_pass      ("pass"     ,       "mqtt pass"  , mqtt_pass  , 20);
WiFiManagerParameter custom_maxPower       ("maxPower" ,       "max power[W]" , maxPower  , 6);
WiFiManagerParameter custom_shelly_url     ("shelly_url",      "grid power http get shelly url i.e http://shelly-ip/status"   , shelly_url      , 100);
WiFiManagerParameter custom_json_keys      ("json_keys",       "Json Keys i.e: < meters,0,power > for Shelly 1PM"             , json_keys       , 60);
WiFiManagerParameter custom_shelly_interval("shelly_interval", "http-get grid power shelly interval [ms]"         , shelly_interval , 16);

bool shouldSaveConfig = false;                     //flag for saving data if WifiManager has changed custom parameters
bool resetConfig      = false;                     //flag to reset config if D0 is connected to D5 (Wemos Mini Pinout) on boot
bool zepc_enable      = true;                      //ZeroExportController enabled

#define STATUS_LED 2                               //LED Pin = 2 for ESP12F/Wemos D1 mini clone

ModbusMaster   node;
#define AC_REG   0                                 //Modbus register address to write AC_Setpoint
#define DAC_REG  4                                 //Modbus register address to write DAC value
#define CAL_REG  5                                 //Modbus register address to write calibration step <1: start calibration> <99: reset calibration LUT to default>

const uint8_t  id                     = 1;         //Modbus RTU slave id of Sun GTIL2 interface

uint16_t       set_ac_power           = 0;         //Sun GTIL2 AC_Setpoint       [W*10]
uint16_t       ac_power               = 0;         //Sun GTIL2 AC_Display output [W*10]
uint16_t       vgrid                  = 0;         //Sun GTIL2 grid voltage      [V*10]
uint16_t       vbat                   = 0;         //Sun GTIL2 battery voltage   [V*10]
uint16_t       dac                    = 0;         //Sun GTIL2 DAC value
uint16_t       cal_step               = 0;         //Sun GTIL2 calibration step
uint16_t       temp                   = 0;         //Sun GTIL2 Temperature       [°C]

float          shelly_power           = 0;         //Shelly Power                [W]
int            shelly_err_cnt         = 0;         //Shelly MOdbus read error counter

uint16_t       t2n_modbus             = 1300;      //Time in [ms] to next modbus read from SUN GTIL2
uint16_t       t2n_shelly             = 500;       //Time in [ms] to next http read from shelly
uint16_t       t2n_mqtt               = 1300;      //Time in [ms] to next mqtt publish
uint16_t       t2n_zepc               = 500;       //Time in [ms] to next ZeroExport Controller update

unsigned long  previousMillis_modbus        = 0;   //Counter to next modbus poll
unsigned long  previousMillis_shelly        = 0;   //Counter to next shelly poll
unsigned long  previousMillis_led           = 0;   //Counter to next led toggle (every 1000ms)
unsigned long  previousMillis_mqtt          = 0;   //Counter to next mqtt publish (every 5000ms)
unsigned long  previousMillis_zepc          = 0;   //Counter to next ZeroExport Controller update (every 500ms)

void handleNotFound() {server.send(404, "text/plain", "Not found");}
void saveConfigCallback () {Serial.println("Should save config"); shouldSaveConfig = true;} //callback notifying us of the need to save config
//------------------------------------------------------------------------------------------- 
float avgPower      = 0;                                                    //Averaged setpoint inverter power [W](grid+inverter)
int   ZeroExportController(float inverter_power, float grid_power){         //Zero export controller calculates a new setpoint power in W for the 
                                                                            //inverter from the actual inverter power and the actual power from the grid
                                                                            //Zero export controller variables
  const uint8_t  grid_offset            = 50;                               //Power [W] which you still want to get from the grid
  const uint8_t  grid_min               = 25;                               //Minimum power [W] from grid
  const uint8_t  grid_max               = 75;                               //Maximum power [W] from grid
  const uint8_t  avgcnt                 = 60;                               //number of averaged values
  uint16_t       t2np                   = 5000;                             //give inverter time before sending next value (time2next Power) = 5000ms
  unsigned long  previousMillis_t2np    = 0;                                //Counter to next inverter power change (every 5000ms)    

  float sumPower = (inverter_power) + grid_power - grid_offset;             //calculate desired power (Disp_ACPower+sdm630-grid_offset)
  if (sumPower < grid_offset) sumPower = 0;
  avgPower = (((avgcnt-1)*avgPower) + sumPower)/avgcnt;                     //calculate avgPower from 59*old_values + 1*new_value
  if ( (grid_power < grid_min) ||                                           //if (grid<grid_min) OR
      ((grid_power > grid_max) && (millis()-previousMillis_t2np > t2np)) ){ //(grid>grid_max) AND Time2NextPower(5s) ->Update Setpoint inverter Power
    if (grid_power < grid_min) avgPower=sumPower;                           //if power from grid < grid min -> correct inverter power instantly
    previousMillis_t2np = millis();
    return 1;                                                               
  }                                                          
  return 0;
}
//------------------------------------------------------------------------------------------- 
void readCustomParameters(void){
  Serial.println("Reading from eeprom:");
  StaticJsonDocument<EEPROM_SIZE> json;
  EEPROM.begin(EEPROM_SIZE);
  EepromStream eepromStream(0, EEPROM_SIZE);
  DeserializationError error = deserializeJson(json,eepromStream);
  EEPROM.end();
  serializeJsonPretty(json,Serial); Serial.println();     
  if(json.overflowed()) Serial.println("Error: Json overflowed!");
  if (!error) {
    if (json["mqtt_server"])     strcpy(mqtt_server,    json["mqtt_server"]);    else Serial.println("couldn't read mqtt_Server");
    if (json["mqtt_port"])       strcpy(mqtt_port,      json["mqtt_port"]);      else Serial.println("couldn't read mqtt_port");
    if (json["mqtt_user"])       strcpy(mqtt_user,      json["mqtt_user"]);      else Serial.println("couldn't read mqtt_user");
    if (json["mqtt_pass"])       strcpy(mqtt_pass,      json["mqtt_pass"]);      else Serial.println("couldn't read mqtt_pass");
    if (json["shelly_url"])      strcpy(shelly_url,     json["shelly_url"]);     else Serial.println("couldn't read shelly_url");
    if (json["json_keys"])       strcpy(json_keys,      json["json_keys"]);      else Serial.println("couldn't read json_keys");
    if (json["shelly_interval"]) strcpy(shelly_interval,json["shelly_interval"]);else Serial.println("couldn't read shelly_interval");                                     
    if (json["maxPower"])        strcpy(maxPower,       json["maxPower"]);       else Serial.println("couldn't read maxPower");
    if (json["ip"])              strcpy(static_ip,      json["ip"]);             else Serial.println("couldn't read ip");
    if (json["gateway"])         strcpy(static_gw,      json["gateway"]);        else Serial.println("couldn't read gateway");
    if (json["subnet"])          strcpy(static_sn,      json["subnet"]);         else Serial.println("couldn't read subnet");
  } else {Serial.print("ERROR: no custom parameters in config. Error:"); Serial.println(error.c_str());}
}
//------------------------------------------------------------------------------------------- 
void writeCustomParameters(void){                       //save custom parameters to FS
  if (shouldSaveConfig) {                               //saving config
    strcpy(mqtt_server  ,   custom_mqtt_server.getValue()); //read updated parameters
    strcpy(mqtt_port    ,   custom_mqtt_port.getValue());
    strcpy(mqtt_user    ,   custom_mqtt_user.getValue());
    strcpy(mqtt_pass    ,   custom_mqtt_pass.getValue()); 
    strcpy(shelly_url   ,   custom_shelly_url.getValue());
    strcpy(json_keys    ,   custom_json_keys.getValue());
    strcpy(shelly_interval, custom_shelly_interval.getValue());
    strcpy(maxPower     ,   custom_maxPower.getValue()); 
    StaticJsonDocument<EEPROM_SIZE> json;
    json["mqtt_server"]     = mqtt_server;
    json["mqtt_port"]       = mqtt_port;
    json["mqtt_user"]       = mqtt_user;
    json["mqtt_pass"]       = mqtt_pass;
    json["shelly_url"]      = shelly_url;
    json["json_keys"]       = json_keys;
    json["shelly_interval"] = shelly_interval;
    json["maxPower"]        = maxPower;
    json["ip"]              = WiFi.localIP().toString();
    json["gateway"]         = WiFi.gatewayIP().toString();
    json["subnet"]          = WiFi.subnetMask().toString(); 
    Serial.println("Writing to eeprom:");
    EEPROM.begin(EEPROM_SIZE);
    EepromStream eepromStream(0, EEPROM_SIZE);
    serializeJson(json, eepromStream);
    serializeJsonPretty(json,Serial); Serial.println();
    if(json.overflowed()) Serial.println("Error: Json overflowed!");
    eepromStream.flush();
    EEPROM.end();
    shouldSaveConfig = false;
  }
}
//------------------------------------------------------------------------------------------- 
void WifiManagerStart(void){                           //WifiManger 
  WiFiManager wm;
  wm.setBreakAfterConfig(true);                        //Save config even if wifi couldn't connect
  wm.setSaveConfigCallback(saveConfigCallback);        //set config save notify callback
  IPAddress _ip, _gw, _sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);
  wm.setSTAStaticIPConfig(_ip, _gw, _sn);
  wm.addParameter(&custom_mqtt_server);                //Add custum parameter to captive portal
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);
  wm.addParameter(&custom_shelly_url);
  wm.addParameter(&custom_json_keys);
  wm.addParameter(&custom_shelly_interval);
  wm.addParameter(&custom_maxPower);
  if (resetConfig) wm.resetSettings();                 //Reset WifiManager Settings if WEMOS D5 connected to GND on boot
  if (!wm.autoConnect("Trucki2Shelly Gateway")) {      //if not connected to wifi, start AP named "Trucki2Shelly Gateway"
      writeCustomParameters();                         //if not connected save parameters anyway
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.restart();                                   //reset and try again, or maybe put it to deep sleep
      delay(5000);
  }
  Serial.println("connected...yeey :)");               //if you get here you have connected to the WiFi
  wm.setDebugOutput(false);                            //Disable WifiManger Serial Output    
}
//------------------------------------------------------------------------------------------- 
void handleRoot() {  //WebSite
  String inv_color    = "red";
  String shelly_color = "red";
  String mqtt_color   = "red";
  String zepc_color   = "red";
  if (t2n_modbus < 20000) inv_color    = "green";
  if (t2n_shelly < 20000) shelly_color = "green";
  if (t2n_mqtt   < 20000) mqtt_color   = "green"; 
  if (zepc_enable == true) zepc_color  = "green";
    
  String html = "<html><head><meta http-equiv=\"refresh\" content=\"2\">"
              + String("<p style=\"color:")    + String(inv_color)          + String(";\">")
              + String("AC setpoint[W*10]:")   + String(set_ac_power)       + String("<br>")
              + String("AC display[W*10]:")    + String(ac_power)           + String("<br>")
              + String("VGrid[V*10]:")         + String(vgrid)              + String("<br>")
              + String("VBat[V*10]:")          + String(vbat)               + String("<br>")
              + String("DAC:")                 + String(dac)                + String("<br>")
              + String("Cal. step:")           + String(cal_step)           + String("<br>")
              + String("Temperature[C]:")      + String(temp)               + String("<br>")
              + String("Max power[W]:")        + String(maxPower)           + String("<br>")
              + String("</p>") 
              + String("<p style=\"color:")    + String(shelly_color)       + String(";\">")
              + String("Shelly power[W]:")     + String(shelly_power)       + String("<br>")
              + String("Shelly url:")          + String(shelly_url)         + String("<br>")
              + String("Json Keys:")           + String(json_keys)          + String("<br>")
              + String("Shelly_interval[ms]:") + String(shelly_interval)    + String("<br>")
              + String("</p>")
              + String("<p style=\"color:")    + String(zepc_color)         + String(";\">")
              + String("ZeroExportController:")+ String(zepc_enable)        + String("<br>")
              + String("</p>") 
              + String("<p style=\"color:")    + String(mqtt_color)         + String(";\">")
              + String("MQTT server:")         + String(mqtt_server)        + String("<br>")
              + String("MQTT port:")           + String(mqtt_port)          + String("<br>")
              + String("MQTT user:")           + String(mqtt_user)          + String("<br>")
              + String("MQTT ACSetpointOVR:")  + String(mqtt_ACSetpointOVR) + String("<br>")
              + String("MQTT DACOVR:")         + String(mqtt_DACOVR)        + String("<br>")
              + String("MQTT CALSTEPOVR:")     + String(mqtt_CALSTEPOVR)    + String("<br>")
              + String("MQTT SHELLYPOWEROVR:") + String(mqtt_SHELLYPOWEROVR)+ String("<br>")                                          
              + String("</p>") 
              + "</body></html>";
  server.send(200, "text/html", html);    
}
//------------------------------------------------------------------------------------------- 
int mqtt_publish(void) {
  if(!mqttClient.connected()) mqttClient.connect(mqtt_server, atoi(mqtt_port));                            //if not connected to mqtt broker
  if(mqttClient.connected()){                                                                              //if connected, publish data
    mqttClient.beginMessage(MQTT_ACSETPOINT);  mqttClient.print((float)set_ac_power/10); mqttClient.endMessage();
    mqttClient.beginMessage(MQTT_ACDISPLAY);   mqttClient.print((float)ac_power/10);     mqttClient.endMessage();
    mqttClient.beginMessage(MQTT_VGRID);       mqttClient.print((float)vgrid/10);        mqttClient.endMessage(); 
    mqttClient.beginMessage(MQTT_VBAT);        mqttClient.print((float)vbat/10);         mqttClient.endMessage();
    mqttClient.beginMessage(MQTT_DAC);         mqttClient.print(dac);                    mqttClient.endMessage();
    mqttClient.beginMessage(MQTT_CALSTEP);     mqttClient.print(cal_step);               mqttClient.endMessage(); 
    mqttClient.beginMessage(MQTT_TEMPERATURE); mqttClient.print(temp);                   mqttClient.endMessage(); 
    mqttClient.beginMessage(MQTT_SHELLYPOWER); mqttClient.print(shelly_power);           mqttClient.endMessage();        
  } else return 0;
  return 1; 
}
//------------------------------------------------------------------------------------------- 
void onMqttMessage (int messageSize) {
  //Save Topic
  String topic = mqttClient.messageTopic();

  //Check if payload is int and convert it to int
  String payload_str;
  int payload_int;
  int i = 0;
   while (mqttClient.available()) {
    char received = (char)mqttClient.read();
    payload_str.concat( received);
    i++;
  }
  if (isValidNumber(payload_str)) payload_int = atoi(payload_str.c_str());

  if (topic == MQTT_ACSETPOINTOVR){                                       //Process AC_Setpoint_override mqtt message
    if (payload_int <0) payload_int = 0;                                  //AC_Setpoint musst be > 0
    if (payload_int > atoi(maxPower)) payload_int = atoi(maxPower);       //AC_Setpoint musst be < maxPower
    mqtt_ACSetpointOVR = payload_str;                                     //save payload for website
    zepc_enable = false;                                                  //ZeroExportController disabled
    writeGTIL(AC_REG,(float)payload_int);                                 //Modbus send AC_Setpoint to SUN
  }
  
  if (topic == MQTT_DACOVR){                                              //Process DAC_override mqtt message
    if (payload_int <0) payload_int = 0;                                  //DAC value musst be > 0
    if (payload_int > 65535) payload_int = 65535;                         //DAC value musst be <= 65535
    mqtt_DACOVR = payload_str;                                            //save payload for website
    zepc_enable = false;                                                  //ZeroExportController disabled        
    writeGTIL(AC_REG,0);                                                  //AC_Setpoint musst be 0
    delay(300);
    writeGTIL(DAC_REG,(float)payload_int);                                //Modbus send DAC value
  }
  
  if (topic == MQTT_CALSTEPOVR){                                          //Process Calibration step override mqtt message
    if (payload_int <0) payload_int = 0;                                  //cal. step value musst be > 0
    if (payload_int > 65535) payload_int = 65535;                         //cal. step value musst be <= 65535
    mqtt_CALSTEPOVR = payload_str;                                        //save payload for website 
    zepc_enable = false;                                                  //ZeroExportController disabled        
    writeGTIL(AC_REG,0);                                                  //AC_Setpoint musst be 0
    delay(300);
    writeGTIL(DAC_REG,0);                                                 //DAC musst be 0
    delay(300);
    writeGTIL(CAL_REG,(float)payload_int);                                //Modbus send Cal. Step value override                                          
  }

  if (topic == MQTT_SHELLYPOWEROVR){                                      //Shelly power override. Shelly url for http get request musst be empty
    mqtt_SHELLYPOWEROVR = payload_str;                                    //save payload for website         
    zepc_enable = true;                                                   //ZeroExportController enabled    
    shelly_power = (float) payload_int;
  }
}
//------------------------------------------------------------------------------------------- 
 boolean isValidNumber(String str){
  for(byte i=0;i<str.length();i++)
    if(!isDigit(str.charAt(i))) return false;
  return true;
}
//------------------------------------------------------------------------------------------- 
int readShelly(int debug)  //Get power value from Shelly via http get & deserializeJson
{
  DynamicJsonDocument doc(2048);                                                                                            //Json document for http response
  StaticJsonDocument<256>  input_json, output_json;                                                                         //Json document for Token/Key 1-4
  HTTPClient http;  
    if (http.begin(httpClient, shelly_url))                                                                                    //open http client with htt_get url 
    {    
      int httpCode = http.GET();                                                                                              //get data from http_get url
      if(debug){Serial.print("http code: "); Serial.println(httpCode);}
      if (httpCode > 0)                                                                                                       //if http_get was successful
      {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)  
        {
          if(debug){Serial.print("http received: "); Serial.println(http.getString());}
          DeserializationError error = deserializeJson(doc, http.getString() );
        
          if (!error) {                 
             if (debug) {Serial.print("Json_keys: "); Serial.println(json_keys);}
             char json_keys_cpy[sizeof(json_keys)];                                                                           //copy json_keys i.e. meters,0,power
             for (int i=0;i<sizeof(json_keys);i++)json_keys_cpy[i]=json_keys[i];  
             char *token = strtok(json_keys_cpy, ",");                                                                        //read first token from json_keys_cpy i.e. meters
             if (isValidNumber(token)) input_json = doc[atoi(token)]; else input_json = doc[token];                           //copy json data for first token to input_json
             if (debug) {Serial.print("Token: "); Serial.println(token); serializeJson(input_json,Serial);Serial.println();}  
             token=strtok(NULL, ",");                                                                                         //get 2nd token i.e. 0  
             while (token != NULL) {
               if (isValidNumber(token)) output_json = input_json[atoi(token)]; else output_json = input_json[token];         //copy json data for 2nd, 3rd, 4th token to input_json
               input_json  = output_json;
               if (debug) {Serial.print("Token: "); Serial.println(token); serializeJson(input_json,Serial);Serial.println();}    
               token=strtok(NULL, ",");                                                                                       //get token 3rd, 4th
             }
             shelly_power = input_json.as<float>();                                                                           //copy json power to float shelly_power
             if (debug) {Serial.print("Shelly power: "); Serial.println(shelly_power);}
          }else return 0; //Serial.println("json deserializeJson() failed");
        }else return 0; //Serial.println("wrong http code");
      }else return 0; //Serial.println("http get failed");    
      http.end();
    }else return -1; //Serial.println("HTTP Unable to connect");
  return 1; //No Error
}
//------------------------------------------------------------------------------------------- 
int readGTIL(void){                                                       //Read values via UART,Modbus RTU from GTIL interface pcb
  if (node.readInputRegisters(7, 1) == node.ku8MBSuccess){
    temp         = node.getResponseBuffer(0);                             //Read Register 7 (Temp) first due to compatibility to older firmwares
  }
  if (node.readInputRegisters(0, 6) == node.ku8MBSuccess){                //Read register[0-5]
    set_ac_power = node.getResponseBuffer(0);
    ac_power     = node.getResponseBuffer(1);
    vgrid        = node.getResponseBuffer(2);
    vbat         = node.getResponseBuffer(3);
    dac          = node.getResponseBuffer(4);
    cal_step     = node.getResponseBuffer(5); 
    return 1;  
  } else return 0;  //Read failed
}
//------------------------------------------------------------------------------------------- 
int writeGTIL(int reg, float value) {                                       //Modbus write ac_setpoint power to Sun GTIL2               

  uint16_t U16_value = 0;
  
  if (reg == AC_REG){                                                              //AC_Setpoint
    if (value < 0) value = 0;
    U16_value = (uint16_t)value*10;
    if (U16_value == set_ac_power) return 0;                                       //nothing to change, do not send
  }  

  if (reg == DAC_REG){                                                             //DAC
    if (value < 0) value = 0;
    U16_value = (uint16_t)value;    
    if (U16_value == dac) return 0;                                                //nothing to change, do not send
  }  

  if (reg == CAL_REG){                                                             //Calibration Step
    if (value < 0) value = 0;
    U16_value = (uint16_t)value;
    if (U16_value == cal_step) return 0;                                           //nothing to change, do not send
  }  

  if (node.writeSingleRegister(reg,U16_value) == node.ku8MBSuccess) return 1;      //Modbus send
  return 0;
} 
//------------------------------------------------------------------------------------------- 
void setup(void) {
  //Pin config
  pinMode(16, OUTPUT);                                                   //WEMOS D0, GPIO16
  pinMode(14, INPUT_PULLUP);                                             //WEMOS D5, GPIO16
  digitalWrite(16, LOW);                                                 //D0 = GND
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);                                         //Turn on Status LED
  
  //Serial init 
  Serial.begin(9600);                                                    //Open UART
  delay(200);
  Serial.println("Starting up...");
  Serial.print("Trucki2Shelly Gateway V: "); Serial.println(VERSION);
  
  //Reset config?
  if (digitalRead(14) == LOW){                                           //If D5 connected to GND (or D0) -> reset config
    delay(500);
    if (digitalRead(14) == LOW){
      Serial.println("!!! CONFIG RESET !!!");
      Serial.println("EEPROM wipe");
      EEPROM.begin(EEPROM_SIZE);
      for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
      EEPROM.commit();
      delay(500);
      EEPROM.end();      
      resetConfig = true;
    }
  } 

  //Read parameters from config
  readCustomParameters();                                                //read custom parameter from Filesystem 
  //Start wifi
  WifiManagerStart();                                                    //Wifi Manager start
  //Save changed parameters
  writeCustomParameters();                                               //Check if Wifimanager changed custom parameters and write them to eeprom 
  
  t2n_shelly = atoi(shelly_interval);
  
  //Webserver init
  MDNS.begin("trucki2shellygateway");                                    //http://trucki2shellygateway.local
  server.on("/", handleRoot);                                            // -- Set up required URL handlers on the web server.
  server.onNotFound(handleNotFound);
  server.begin();  
                     
  //MQTT init
  mqttClient.setId("Trucki2Shelly-Gateway");                             //set mqtt broker/server id
  if (*mqtt_user != 0)
    mqttClient.setUsernamePassword(mqtt_user,mqtt_pass);                 //set mqtt user/password
  if (*mqtt_server != 0)
    if (!mqttClient.connect(mqtt_server, atoi(mqtt_port))){              //connect to mqtt server with mqtt port
      Serial.print("mqtt failed: ");
      Serial.println(mqttClient.connectError());
    }  
    else{
      Serial.println("mqtt connected"); 
      mqttClient.onMessage(onMqttMessage);
      if (mqttClient.subscribe(MQTT_ACSETPOINTOVR))  Serial.print("mqtt subscribed to "); Serial.println(MQTT_ACSETPOINTOVR);
      if (mqttClient.subscribe(MQTT_DACOVR))         Serial.print("mqtt subscribed to "); Serial.println(MQTT_DACOVR);
      if (mqttClient.subscribe(MQTT_CALSTEPOVR))     Serial.print("mqtt subscribed to "); Serial.println(MQTT_CALSTEPOVR);
      if (mqttClient.subscribe(MQTT_SHELLYPOWEROVR)) Serial.print("mqtt subscribed to "); Serial.println(MQTT_SHELLYPOWEROVR);
      //mosquitto_pub -h 192.168.1.225 -u root -P homeassistant1 -t T2SG/Shellypower_override -m "99"
    }

  //Http get grid power test
  if(readShelly(1))Serial.println("Http get grid power connected");
  else Serial.println("Http get grid power read failed");

  //Modbus init 
  Serial.println("Debug output stop, starting Modbus to SUN GTIL2");
  node.begin(id, Serial);                                                //id=Modbus slave/client id of the interface pcb (Sun GTIL2)

  digitalWrite(STATUS_LED, HIGH);                                        //Turn off Status LED
}
//------------------------------------------------------------------------------------------- 
void loop(void) { 

  MDNS.update();
  server.handleClient();                                                  //WebServer Update
  if (*mqtt_server != 0) mqttClient.poll();                               //mqtt poll (keep a live, etc...)  

  //SUN GTIL2 read values
  if ( (millis()-previousMillis_modbus) >= t2n_modbus){                   //Get new values from Sun GTIL2 every 1300ms
    if (!readGTIL()) {                                                    //set all values=0 on error
      set_ac_power = 0; ac_power     = 0;
      vgrid        = 0; vbat         = 0;
      dac          = 0; cal_step     = 0;
      temp         = 0;
      t2n_modbus = 20000;                                                 //If SUN GTIL2 was NOT reached retry in 20s 
    }else t2n_modbus = 1300;                                              //If SUN GTIL2 was reached reconnect in 1.3s
    previousMillis_modbus= millis();                                   
  }
 
  //Http get read grid power & calculate GTIL2 power
  if((*shelly_url != 0)&&((millis()-previousMillis_shelly)>=t2n_shelly)){ //every 500ms
    if(readShelly(0) ) {                                                  //Get power value from Shelly
      shelly_err_cnt = 0;
      t2n_shelly = atoi(shelly_interval);                                 //reconnect to shelly in 500ms
    }else {
      shelly_err_cnt++;                                                   //Error read Shelly 
      if (shelly_err_cnt >= 3) shelly_power = 0;                                                 //set shelly_power=0 on Error      
      t2n_shelly = 20000;                                                 //retry to connect to shelly in 20s 
    }
    previousMillis_shelly = millis();
  }

  //ZeroExportController
  if ( (millis()-previousMillis_zepc) >= t2n_zepc){                       //ZeroExportController update every 500ms
    if (zepc_enable == true ){                                            //ZeroExportController enabled?
      if (ZeroExportController((float)ac_power/10, shelly_power) ){
        uint16_t maxPower_u16 = atoi(maxPower);
        if (avgPower > maxPower_u16) avgPower = maxPower_u16;             //limit inverter power to maxPower
        writeGTIL(AC_REG,avgPower);    
      }
    }                                                                     
    previousMillis_zepc = millis();
  }
   
  //MQTT publish
  if ((millis()- previousMillis_mqtt) >= t2n_mqtt){                       //publish a mqtt msg every 1300ms
    if (*mqtt_server != 0) 
      if (mqtt_publish()) t2n_mqtt = 1300; else t2n_mqtt = 20000;
    previousMillis_mqtt = millis();
  }                                                   
 
  //LED Toggle
  if ((millis()-previousMillis_led) >= 500){                            //Toggle led every 1s
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));                   //LED Toggle
    previousMillis_led = millis();
  }
}
//------------------------------------------------------------------------------------------- 
