#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <EasyDDNS.h>
#include <ESP8266WiFi.h>
#define ONE_WIRE_BUS 5
#define USE_SERIAL Serial
ESP8266WiFiMulti WiFiMulti;
typedef unsigned char BYTE;
DynamicJsonDocument doc(1024);
int address = 0;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
char temperatureString[6];
String deviceSerial;
boolean onceFlag=true;
float treshold;
float lastTemp;
void setup()
{
    deviceSerial = "pzh00315001";
    USE_SERIAL.begin(115200);
    EEPROM.begin(512);
    for (uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP("pooyan", "12345678");
}
float getTemperature()
{
    float temp;
    do {
        DS18B20.requestTemperatures();
        temp = DS18B20.getTempCByIndex(0);
        delay(170);
    } while (temp == 85.0 || temp == (-127.0));
    return temp;
}
void print(String text)
{
    USE_SERIAL.println(text);
}
void WriteTemperatureThreshold(char* serial)
{
    for (int i = 0; i < 5; i++) {
        writeToEEPROM(serial[i], i);
    }
	writeToEEPROM('\0',5);
    USE_SERIAL.println("[EEPROM] temperature threshold wrote!");
    blinkPin(2, 5);
}
void WriteIsReadTemperatureFromIC(char* serial)
{
    writeToEEPROM(serial[0], 6);
	writeToEEPROM('\0',7);
    USE_SERIAL.println("[EEPROM] Is Read Temperature From IC? wrote!");
    blinkPin(2, 5);
}
void WriteIsCheckRelayState(char* serial)
{
    writeToEEPROM(serial[0], 8);
	writeToEEPROM('\0',9);
    USE_SERIAL.println("[EEPROM] is Check Relay State, wrote!");
    blinkPin(2, 5);
}
void decToAscii(char character)
{
    USE_SERIAL.printf("%c", character);
}
void loop()
{
  
    if ((WiFiMulti.run() == WL_CONNECTED)) {
      checkReset();
      if(onceFlag){
        ReadConfig();
        onceFlag=false;}
        
        float temperature = getTemperature();
        if (temperature > treshold) {
            turnPinOn(4);
        }
        else {
            turnPinOff(4);
        }
        
        dtostrf(temperature, 2, 2, temperatureString);
        String url = temperatureString;
        if(lastTemp!=temperature){
        sendToServer(temperatureString);
        lastTemp=temperature;
        }
        
    }
    delay(300);
}
void turnPinOn(int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}
void turnPinOff(int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}
void blinkPin(int pin, int counter)
{
    int i = 0;
    do {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        delay(200);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        i++;
    } while (i < counter);
}
void ReadConfig()
{
    HTTPClient http;
    USE_SERIAL.print("[HTTP-CONFIG] begin...\n");
    String url = "http://project.pzandian.ir/getConfig.php?serial=" + deviceSerial;
    http.begin(url);
    USE_SERIAL.print("[HTTP-CONFIG] GET...\n");
    int httpCode = http.GET();
    if (httpCode > 0) {
        USE_SERIAL.printf("[HTTP-CONFIG] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            ParseConfig(payload);
        }
    }
    else {
        USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        ESP.reset();
    }
    http.end();
}
void checkReset()
{
    HTTPClient http;
    USE_SERIAL.print("[HTTP-CheckReset] begin...\n");
    String url = "http://project.pzandian.ir/checkReset.php?serial=" + deviceSerial;
    http.begin(url);
    USE_SERIAL.print("[HTTP-CheckReset] GET...\n");
    int httpCode = http.GET();
    if (httpCode > 0) {
        USE_SERIAL.printf("[HTTP-CheckReset] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            deserializeJson(doc, payload);
            JsonObject obj = doc.as<JsonObject>();
            String resetStatus = obj["status"].as<String>();
            if(resetStatus=="1"){
              print("reseting....");
            ESP.reset();
            return;
            }
            
        }
    }
    else {
        USE_SERIAL.printf("[HTTP-CheckReset] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        ESP.reset();
    }
    http.end();
}
char* ConvertForEEPROM(String data)
{
    int len = data.length() + 1;
    char char_array[len];
    data.toCharArray(char_array, len);
    return char_array;
}
void ParseConfig(String deviceConfig)
{
    USE_SERIAL.println("[PARSER] parsing received data...");
    deserializeJson(doc, deviceConfig);
    JsonObject obj = doc.as<JsonObject>();
    String serverStatus = obj["status"].as<String>();
    String temperatureThreshold = obj["temperatureThreshold"].as<String>();
    String readTemperatureFromIC = obj["readTemperatureFromIC"].as<String>();
    String isCheckRelayState = obj["isCheckRelayState"].as<String>();
    if (serverStatus == "1") {
        WriteTemperatureThreshold(ConvertForEEPROM(temperatureThreshold));
        WriteIsReadTemperatureFromIC(ConvertForEEPROM(readTemperatureFromIC));
		WriteIsCheckRelayState(ConvertForEEPROM(isCheckRelayState));
    }
    else {
        print("[Server] You do not have a license from the server");
		ESP.reset();
    return;
    }
    readConfigFromEEPROM();
	
}
void readConfigFromEEPROM(){
	ReadTreshold();
}
void ReadTreshold(){
	treshold=read_String(0).toFloat();
  print(read_String(0));
}
void sendToServer(String url)
{
    HTTPClient http;
    USE_SERIAL.print("[HTTP] begin...\n");
    url = "http://project.pzandian.ir/iot/raspberryPiDataReciever.php?iotSave&id=" + deviceSerial + "&value=" + url;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
        USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
    }
}

void stringToByteArray(char* input, BYTE* output)
{
    int loop;
    int i;
    loop = 0;
    i = 0;
    while (input[loop] != '\0') {
        output[i++] = input[loop++];
    }
}
/*byte readEEPROM(int address)
{
    return EEPROM.read(address);
}*/
void writeToEEPROM(byte value, int address)
{
    EEPROM.write(address, value);

    EEPROM.commit();
}

String read_String(char add)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(add);
  while(k != '\0' && len<500)   //Read until null character
  {    
    k=EEPROM.read(add+len);
    data[len]=k;
    len++;
  }
  data[len]='\0';
  return String(data);
}
