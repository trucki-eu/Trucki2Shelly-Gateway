/*
 * Trucki2Shelly Gateway
 * ---------------------
 * This Arduino code was written for a ESP8266 WEMOS D1 mini. Its purpose is to read the total
 * power from a Shelly 3EM and send it via UART to Trucki's RS485 interface pcb for 
 * SUN GTIL2-1/2000 MPPT inverter. J1-J5 have to be open on the RS485 interface pcb (UART, ID:1).
 * 
 * The total power is filterd (ZeroExportController) before it is send to the inverter. The goal
 * of the ZeroExportController is to keep the Shelly total power at ~50W. The inverter power 
 * rises slowly over ~30s and drops fast (~1s). 
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
 * want to publish mqtt data. Use 0.0.0.0 for static IP, subnet and getway if you want to get an
 * IP address from your DHCP server.
 * Once you have finished you can reset the module and check if it connects to your wifi. 
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
 * MQTT client
 * -----------
 * The mqtt client publishes every 5s the following topics: T2SG/ACSetpoint, T2SG/ACDisplay,
 * T2SG/VGrid, T2SG/VBat, T2SG/DAC, T2SG/Calstep, T2SG/Temperature, T2SG/Shellypower.
 * You can use the MQTT client to log data i.e. with HomeAssistant.
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
 * 
 * //------------------------------------------------------------------------------------------- 
 * 1.00 28.10.2022 : 1st version
 * //------------------------------------------------------------------------------------------- 
*/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //Benoit Blanchon ARDUINOJSON_VERSION_MAJOR >= 6
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ModbusMaster.h>         //https://github.com/4-20ma/ModbusMaster - by DocWalker
#include <ArduinoMqttClient.h>

DNSServer dnsServer;
ESP8266WebServer server(80);

WiFiClient espClient;
MqttClient mqttClient(espClient);

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "192.168.1.225";
char mqtt_port[6]    = "1883";
char mqtt_user[20]   = "mqtt_user";
char mqtt_pass[20]   = "mqtt_pass";
char shelly_ip[16]   = "192.168.1.217";
char maxPower[6]     = "850";
char static_ip[16]   = "192.168.1.132";
char static_gw[16]   = "192.168.1.1";
char static_sn[16]   = "255.255.255.0";

WiFiManagerParameter custom_mqtt_server("server"   , "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port  ("port"     , "mqtt port"  , mqtt_port  , 6);
WiFiManagerParameter custom_mqtt_user  ("user"     , "mqtt user"  , mqtt_user  , 20);
WiFiManagerParameter custom_mqtt_pass  ("pass"     , "mqtt pass"  , mqtt_pass  , 20);
WiFiManagerParameter custom_shelly_ip  ("shelly_ip", "Shelly IP"  , shelly_ip  , 16); 
WiFiManagerParameter custom_maxPower   ("maxPower" , "max power[W]" , maxPower  , 6);

bool shouldSaveConfig = false;                     //flag for saving data if WifiManager has changed custom parameters
bool resetConfig      = false;                     //flag to reset config if D0 is connected to D5 (Wemos Mini Pinout) on boot

//#define SHELLY_POWER_PATTERN ["meters"][0]["power"]      //Uncomment for Shelly 1PM
#define SHELLY_POWER_PATTERN ["total_power"]           //Uncomment for Shelly 3EM

#define STATUS_LED 2                               //LED Pin = 2 for ESP12F/Wemos D1 mini clone

ModbusMaster   node;
const uint8_t  id                     = 1;         //Modbus RTU slave id of Sun GTIL2 interface

uint16_t       set_ac_power           = 0;         //Sun GTIL2 AC_Setpoint       [W*10]
uint16_t       ac_power               = 0;         //Sun GTIL2 AC_Display output [W*10]
uint16_t       vgrid                  = 0;         //Sun GTIL2 grid voltage      [V*10]
uint16_t       vbat                   = 0;         //Sun GTIL2 battery voltage   [V*10]
uint16_t       dac                    = 0;         //Sun GTIL2 DAC value
uint16_t       cal_step               = 0;         //Sun GTIL2 calibration step
uint16_t       temp                   = 0;         //Sun GTIL2 Temperature       [°C]

int            shelly_power           = 0;         //Shelly Power                [W]
int            shelly_err_cnt         = 0;         //Shelly MOdbus read error counter

uint16_t       t2n_modbus             = 1300;      //Time in [ms] to next modbus read from SUN GTIL2
uint16_t       t2n_shelly             = 500;       //Time in [ms] to next http read from shelly
unsigned long  previousMillis_modbus  = 0;         //Counter to next modbus poll
unsigned long  previousMillis_shelly  = 0;         //Counter to next shelly poll

unsigned long  previousMillis_led     = 0;         //Counter to next led toggle (every 1000ms)
unsigned long  previousMillis_mqtt    = 0;         //Counter to next mqtt publish (every 5000ms)

void handleNotFound() {server.send(404, "text/plain", "Not found");}
void saveConfigCallback () {Serial.println("Should save config"); shouldSaveConfig = true;} //callback notifying us of the need to save config
//------------------------------------------------------------------------------------------- 
float avgPower = 0;                                                         //Averaged setpoint inverter power [W](grid+inverter)
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
  if (resetConfig) SPIFFS.format();                       //clean FS if WEMOS D5 connected to GND
  if (SPIFFS.begin()) {                                   //mounting FS
    if (SPIFFS.exists("/config.json")) {                  //file exists, reading and loading
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {                                   //opened config file
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);      // Allocate a buffer to store contents of the file.
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json,buf.get()); 
        serializeJsonPretty(json,Serial);
        if (!error) {
          if (json["mqtt_server"])strcpy(mqtt_server, json["mqtt_server"]); else Serial.println("couldn't read mqtt_Server");
          if (json["mqtt_port"])  strcpy(mqtt_port,   json["mqtt_port"]);   else Serial.println("couldn't read mqtt_port");
          if (json["mqtt_user"])  strcpy(mqtt_user,   json["mqtt_user"]);   else Serial.println("couldn't read mqtt_user");
          if (json["mqtt_pass"])  strcpy(mqtt_pass,   json["mqtt_pass"]);   else Serial.println("couldn't read mqtt_pass");
          if (json["shelly_ip"])  strcpy(shelly_ip,   json["shelly_ip"]);   else Serial.println("couldn't read shelly_ip");
          if (json["maxPower"])   strcpy(maxPower,    json["maxPower"]);    else Serial.println("couldn't read maxPower");
          if (json["ip"])         strcpy(static_ip,   json["ip"]);          else Serial.println("couldn't read ip");
          if (json["gateway"])    strcpy(static_gw,   json["gateway"]);     else Serial.println("couldn't read gateway");
          if (json["subnet"])     strcpy(static_sn,   json["subnet"]);      else Serial.println("couldn't read subnet");
        } else Serial.println("no custom parameters in config");
        configFile.close();
      } else Serial.println("failed to load json config");
    }
  } else Serial.println("failed to mount FS");
}
//------------------------------------------------------------------------------------------- 
void writeCustomParameters(void){                       //save custom parameters to FS
  if (shouldSaveConfig) {                               //saving config
    strcpy(mqtt_server, custom_mqtt_server.getValue()); //read updated parameters
    strcpy(mqtt_port  , custom_mqtt_port.getValue());
    strcpy(mqtt_user  , custom_mqtt_user.getValue());
    strcpy(mqtt_pass  , custom_mqtt_pass.getValue()); 
    strcpy(shelly_ip  , custom_shelly_ip.getValue()); 
    strcpy(maxPower   , custom_maxPower.getValue());       
    DynamicJsonDocument json(1024);
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["mqtt_user"]   = mqtt_user;
    json["mqtt_pass"]   = mqtt_pass;
    json["shelly_ip"]   = shelly_ip;
    json["maxPower"]    = maxPower;   
    json["ip"]          = WiFi.localIP().toString();
    json["gateway"]     = WiFi.gatewayIP().toString();
    json["subnet"]      = WiFi.subnetMask().toString();
    File configFile = SPIFFS.open("/config.json", "w");
    if (configFile){ 
      serializeJsonPretty(json,Serial);
      serializeJson(json,configFile);
      configFile.close();
    } else Serial.println("failed to open config file for writing");
    shouldSaveConfig = false;
  }
}
//------------------------------------------------------------------------------------------- 
void WifiManagerStart(void){                           //WifiManger 
  WiFiManager wm;
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
  wm.addParameter(&custom_shelly_ip);
  wm.addParameter(&custom_maxPower);
  if (resetConfig) wm.resetSettings();                 //Reset WifiManager Settings if WEMOS D5 connected to GND on boot
  
  if (!wm.autoConnect("Trucki2Shelly Gateway")) {      //if not connected to wifi, start AP named "Trucki2Shelly Gateway"
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
  String html = "<html><head><meta http-equiv=\"refresh\" content=\"2\">"
              + String("AC Setpoint[W*10]:") + String(set_ac_power) + String("<br>")
              + String("AC Display[W*10]:")  + String(ac_power)     + String("<br>")
              + String("VGrid[V*10]:")       + String(vgrid)        + String("<br>")
              + String("VBat[V*10]:")        + String(vbat)         + String("<br>")
              + String("DAC:")               + String(dac)          + String("<br>")
              + String("Cal. Step:")         + String(cal_step)     + String("<br>")
              + String("Temperature[C]:")    + String(temp)         + String("<br>") 
              + String("Shelly Power[W]:")   + String(shelly_power) + String("<br>")
              + String("Shelly IP:")         + String(shelly_ip)    + String("<br>")
              + String("Max Power[W]:")      + String(maxPower)     + String("<br>")
              + String("MQTT Server:")       + String(mqtt_server)  + String("<br>")
              + String("MQTT Port:")         + String(mqtt_port)    + String("<br>")
              + String("MQTT User:")         + String(mqtt_user)    + String("<br>")
              + "</body></html>";
  server.send(200, "text/html", html);    
}
//------------------------------------------------------------------------------------------- 
int mqtt_publish(void) {
    if(!mqttClient.connected()) mqttClient.connect(mqtt_server, atoi(mqtt_port));                            //if not connected to mqtt broker
    
    if(mqttClient.connected()){                                                                              //if connected, publish data
      mqttClient.beginMessage("T2SG/ACSetpoint");  mqttClient.print(set_ac_power/10); mqttClient.endMessage();
      mqttClient.beginMessage("T2SG/ACDisplay");   mqttClient.print(ac_power/10);     mqttClient.endMessage();
      mqttClient.beginMessage("T2SG/VGrid");       mqttClient.print(vgrid/10);        mqttClient.endMessage(); 
      mqttClient.beginMessage("T2SG/VBat");        mqttClient.print(vbat/10);         mqttClient.endMessage();
      mqttClient.beginMessage("T2SG/DAC");         mqttClient.print(dac);             mqttClient.endMessage();
      mqttClient.beginMessage("T2SG/Calstep");     mqttClient.print(cal_step);        mqttClient.endMessage(); 
      mqttClient.beginMessage("T2SG/Temperature"); mqttClient.print(temp);            mqttClient.endMessage(); 
      mqttClient.beginMessage("T2SG/Shellypower"); mqttClient.print(shelly_power);    mqttClient.endMessage();        
    } else return 0;
    return 1;   
}
//------------------------------------------------------------------------------------------- 
int readShelly(void)  //Get power value from Shelly via http get & deserializeJson
{
  DynamicJsonDocument doc(2048);
  HTTPClient http;
  String shellyapiurl_str = "http://" + String(shelly_ip) + "/status";
  const char *shellyapiurl = shellyapiurl_str.c_str();
  if (http.begin(espClient, shellyapiurl)) 
  {  
    int httpCode = http.GET();
    if (httpCode > 0) 
    {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) 
      {
        DeserializationError error = deserializeJson(doc, http.getString() );
        if (!error) {
          shelly_power = doc SHELLY_POWER_PATTERN;
        }else return 0; //Serial.println("json deserializeJson() failed");
      }else return 0; //Serial.println("wrong http code");
    } else return 0; //Serial.println("http get failed");
    http.end();
  } else return -1; //Serial.println("HTTP Unable to connect");
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
//-----------------------------------------------------------------------------------------------------------------
int writeGTIL(float set_ac_power) {                                       //Modbus write ac_setpoint power to Sun GTIL2               
  if (set_ac_power < 0) set_ac_power = 0;
  uint16_t U16_set_ac_power = (uint16_t)set_ac_power*10;
  if (node.writeSingleRegister(0,U16_set_ac_power) == node.ku8MBSuccess) return 1;
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
  
  //Reset config?
  if (digitalRead(14) == LOW){                                           //If D5 connected to GND (or D0) -> reset config
    delay(500);
    if (digitalRead(14) == LOW){
      Serial.println("!!! CONFIG RESET !!!");
      resetConfig = true;
    }
  } 

  //Read parameters from config
  readCustomParameters();                                                //read custom parameter from Filesystem 
  //Start wifi
  WifiManagerStart();                                                    //Wifi Manager start
  //Save changed parameters
  writeCustomParameters();                                               //Check if Wifimanager changed custom parameters and write them to eeprom 

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
    if (!mqttClient.connect(mqtt_server, atoi(mqtt_port)))               //connect to mqtt server with mqtt port
      Serial.println("mqtt failed");
    else Serial.println("mqtt connected"); 

  //Shelly test
  if(readShelly())Serial.println("Shelly connected");
  else Serial.println("Shelly failed");
  
  //Modbus init 
  Serial.println("Debug output stop, starting Modbus to SUN GTIL2");
  node.begin(id, Serial);                                                //id=Modbus slave/client id of the interface pcb (Sun GTIL2)

  digitalWrite(STATUS_LED, HIGH);                                        //Turn off Status LED
}
//------------------------------------------------------------------------------------------- 
void loop(void) { 
  //Webserver
  MDNS.update();
  server.handleClient();                                                  //WebServer Update

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
 
  //Shelly read power & calculate GTIL2 power
  if ( (millis()-previousMillis_shelly) >= t2n_shelly){                   //every 500ms
    if(readShelly() ) {                                                   //Get power value from Shelly
      shelly_err_cnt = 0;
      if (ZeroExportController((float)ac_power/10, (float)shelly_power) ){
        uint16_t maxPower_u16 = atoi(maxPower);
        if (avgPower > maxPower_u16) avgPower = maxPower_u16;             //limit inverter power to maxPower
        writeGTIL(avgPower);    
      }
      t2n_shelly = 500;                                                   //reconnect to shelly in 500ms                                      
    }else {
      shelly_err_cnt++;                                                   //Error read Shelly 
      if (shelly_err_cnt >= 5) {
        writeGTIL(0);
        shelly_power = 0;                                                 //set shelly_power=0 on Error      
      }
      t2n_shelly = 20000;                                                 //retry to connect to shelly in 20s 
    }
    previousMillis_shelly = millis();
  }
   
  //MQTT
  if (*mqtt_server != 0) mqttClient.poll();                               //mqtt poll (keep a live, etc...)
  if ((millis()- previousMillis_mqtt) >= 5000){                           //publish a mqtt msg every 5000ms
    if (*mqtt_server != 0) mqtt_publish();
    previousMillis_mqtt = millis();
  }                                                   
  
  //LED Toggle
  if ((millis()-previousMillis_led) >= 500){                            //Toggle led every 1s
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));                   //LED Toggle
    previousMillis_led = millis();
  }
  
}
//------------------------------------------------------------------------------------------- 
