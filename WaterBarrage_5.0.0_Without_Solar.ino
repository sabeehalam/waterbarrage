#include <LTGeneral.h>
//#include <LTBattery.h>
#include <LTMemory.h>
#include <LTESPmac.h>
#include <LTCredentialsConfigurationV5_0_0.h>
#include <LTwifiAPandSTA.h>
#include <LTPostAndGetRequestV3_0_0.h>
#include <LTJsonParser.h>
#include <LTOta.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

LTGeneral General;
//LTBattery Battery;
LTMemory EEPROMmemory;
LTESPmac ESPMac;
LTCredentialsConfiguration Config;
LTwifiAPandSTA WIFI;
LTPostAndGetRequest HttpRequest;
LTJsonParser JsonParse;
LTOta OtaUpdate;

// C page
const char * AP_SSID = "SmartLinkedthings";
const char * AP_PASSWORD = "123456789";
const char * HUB_SSID = "Smart Home Hub App Test";
const char * HUB_PASSWORD = "123456789";

String DeviceMac, LevelId, OrganizationId, HubId, HubType = "Water";
String values[20];
char WIFI_SSID[32], WIFI_PASSWORD[32];
// END

// Server credentials
//MQTT
String authMethod = "", authToken = "", clientName = "";
// Publish Topics
String evtTopic, evtTopic2, evtTopic3, evtTopic4, evtTopic5;
// Subscribe Topics
String cmdTopic;

String server = "broker.s5.ottomatically.com";

void callback(char* topic, byte* Payload, unsigned int PayloadLength);
WiFiClient wifiClient;
PubSubClient mqttClient((char*)server.c_str(), 1883, callback, wifiClient);

//HTTP
String ServerHost = "https://gateway.s5.ottomatically.com";
String SigninUrl = "/api/v1/hubs/signin";
String DevicePostUrl = "/api/v1/devices/bulk";
String HubUrl = "/api/v1/hubs/";
String OtaUrl, AuthenticationToken, ServerResponseValue;
String CodeVersion = "5.0.0";
// END
String cmdPayload, cmdHubId, cmdDeviceId, cmdEvtType, cmdDeviceName, cmdDeviceValue, cmdDeviceMac, evtPayload;

//Devices
String DevicesType[] = {"WaterTank", "Battery", "SourceIdentifier"};
String DevicesName[] = {"WaterMonitor", "BatteryStatus", "Grid"};
String DevicesDispName[] = {"Water Monitor", "Battery Status",  "Grid"};
String Length[1], Width[1], Height[1], Motor[] = {"false"};

//Gpio's
const byte RESET_ESP_BUTTON_PIN = 15, POWER_LED_PIN = 16;
const byte TRIGGER = 14, ECHO = 12, Relay = 13, Grid_Pin = 5;

//Sonar
float ActualTankSize, SensorDistance;
int TankInPercent;
bool SensorRead = false;

//Battery
long batteryRawValue = 0, batteryValuesConut = 0;
int sensorValue;          // Analog Output of Sensor
float batteryMinRaw = 2.0, batteryMaxRaw = 3.3;
int batteryPercent, oldBatteryPercent;
byte oldValGrid, valGrid;

bool changeBasedPost = true, timeBasedPost = true, firstTimePost = false, sonarRead = true;
String reason;
int loopDelay = 100;

// Data Sending Time
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long readingPostTime = (unsigned long) 1000 * 60 * 0.833;
unsigned long winglePowerTime = (unsigned long) 1000 * 45;

//wingle reset check
bool dataSendCheck = true;

void getConfigRequest(String configData = "");

void setup() {
  Serial.begin(115200);
  pinMode(RESET_ESP_BUTTON_PIN, INPUT);
  pinMode(POWER_LED_PIN, OUTPUT);
  digitalWrite(POWER_LED_PIN, HIGH);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, HIGH);
  Serial.println("\n\nESP Start.");
  delay(winglePowerTime);
  // Hard and Soft Reset
  EEPROMmemory.HardAndSoftReset2(RESET_ESP_BUTTON_PIN);

  // read device Mac
  DeviceMac = ESPMac.mac();
  //  DeviceMac = "a848fadc9fdf";
  Serial.println("\n\nDevice MAC is : " + DeviceMac);
  byte totalDataLength = 6;
  Config.Configuration2(DeviceMac, "SimpleWater", AP_SSID, AP_PASSWORD, values, totalDataLength); // "SimpleWater"
  //  values[0] = "Linkedthings709.";
  //  values[1] = "F4DD396D";
  //  values[2] = "LinkedThings";

  Serial.println("******** Credentials ********");
  for (int j = 0; j < totalDataLength; j++)
    Serial.println("values[" + String(j) + "] = " + values[j]);
  Serial.println("*****************************");

  values[0].toCharArray(WIFI_SSID, values[0].length() + 1);
  values[1].toCharArray(WIFI_PASSWORD, values[1].length() + 1);
  OrganizationId = values[2];
  Length[0] = values[3];
  Width[0] = values[4];
  Height[0] = values[5];

  ActualTankSize = (Height[0].toFloat());

  // This is very important
  setMQTTParameters();

  WIFI.APWithSTAWithoutRestart(HUB_SSID, HUB_PASSWORD, 1, WIFI_SSID, WIFI_PASSWORD, POWER_LED_PIN, false);
  initialMqttConnect();
  String PostPacket = "{\"hubId\":\"" + HubId + "\", \"mac:\":\"" + ESPMac.mac() + "\", \"ssid\":\"" + values[0] + "\", \"pass\":\"" + values[1] + "\", \"type\":\"Info\", \"version\":\"" + CodeVersion + "\"}";
  postEvt(evtTopic2, PostPacket);
}

void loop() {
  batteryPercent = batteryRead(batteryMinRaw, batteryMaxRaw);
  delay(100);
  batteryPercent = Battery.calculateBatteryInFivePercent(batteryPercent);
  valGrid = digitalRead(Grid_Pin);
  delay(100);

  if (sonarRead) {
    mqttClient.unsubscribe((char*) cmdTopic.c_str());
    mqttClient.disconnect();
    Serial.println("\n\nMQTT Client Disconnected and Wingle Power OFF");
    digitalWrite(Relay, LOW);
    delay(1000);
    Serial.println("\nReading Value From Sensor");
    SensorDistance = (Distance(TRIGGER, ECHO, DevicesDispName[0]));
    Serial.println("Sonar Sensor Distance = " + String(SensorDistance) + " inch");

    Serial.println("\n\nReading Done Now Wingle Going To Be Power ON");
    digitalWrite(Relay, HIGH);
    delay(winglePowerTime);

    WIFI.APWithSTAWithoutRestart(HUB_SSID, HUB_PASSWORD, 1, WIFI_SSID, WIFI_PASSWORD, POWER_LED_PIN, false);
    initialMqttConnect();
    if (SensorDistance == 0) {
      Serial.println("\nSonar Sensor not Connected");
      Serial.println("Sonar Sensor Garbage Value = " + String(SensorDistance) + "\n");
      TankInPercent = -1;
      if (SensorRead)
        postLogsEvt(evtTopic4, HubId, DevicesName[0], "Sensor not connected.");
      SensorRead = false;
    } else if ((SensorDistance > 0 && SensorDistance < 8)) {
      if (SensorRead)
        postLogsEvt(evtTopic4, HubId, DevicesName[0], "Sensor gives " + String(SensorDistance));
      SensorRead = false;

    } else if (SensorDistance > ActualTankSize) {
      Serial.println("Overhead Tank Sensor Garbage Value = " + String(SensorDistance) + "\n");
      if (SensorRead)
        postLogsEvt(evtTopic4, HubId, DevicesName[0], "Sensor gives greater then actual tank size value and current value is " + String(SensorDistance));
      SensorRead = false;
    } else {
      while (!mqttClient.connect()) {
        Serial.println("\n\nWingle Power OFF");
        digitalWrite(Relay, LOW);
        delay(1000);
        Serial.println("\nReading Value From Sensor");
        SensorDistance = (Distance(TRIGGER, ECHO, DevicesDispName[0]));
        Serial.println("Sonar Sensor Distance = " + String(SensorDistance) + " inch");

        Serial.println("\n\nWingle Power ON");
        digitalWrite(Relay, HIGH);
        delay(winglePowerTime);
      }

      postEvt(evtTopic, HubId, HubType + "_" + DevicesType[0] + "_Status", HubId + "_" + DevicesName[0], String(SensorDistance), String(SensorDistance), reason);
      delay(100);
      postEvt(evtTopic, HubId, HubType + "_" + DevicesType[1] + "_Status", HubId + "_" + DevicesName[1], String(batteryPercent), String(batteryRawValue), reason);
      delay(100);
      postEvt(evtTopic, HubId, HubType + "_" + DevicesType[2] + "_Status", HubId + "_" + DevicesName[2], String(valGrid), reason);
      delay(100);
      //      oldValGrid = valGrid;
      //      oldBatteryPercent = batteryPercent;
      SensorRead = true;
    }
    sonarRead = false;
  }

  Serial.println("\n------New Readings------");
  Serial.println(DevicesDispName[0] + ": " + String(SensorDistance) + " inches");
  Serial.println(DevicesDispName[1] + ": " + String(batteryPercent) + " %");
  Serial.println(DevicesDispName[2] + " Source: " + String(valGrid));
  Serial.println("------------------------\n");

  // First Time OR Time Based Data Post
  currentMillis = millis();
  if ((timeBasedPost && currentMillis - previousMillis > readingPostTime) || !firstTimePost) {
    previousMillis = currentMillis;
    if (firstTimePost)
      reason = "Time Based";
    else
      reason = "Initial";

    firstTimePost = true;
    sonarRead = true;
  }
}

int batteryRead(float batteryMinRaw, float batteryMaxRaw) {
  batteryRawValue = General.readAnalogSensor();
  float voltage = ((batteryRawValue * 3.3) / 1024); //multiply by two as voltage divider network is 100K & 100K Resistor

  int bat_percentage = General.mapfloat(voltage, batteryMinRaw, batteryMaxRaw, 0, 100); //2.8V as Battery Cut off Voltage & 4.2V as Maximum Voltage
  if (bat_percentage >= 100)
    bat_percentage = 100;
  if (bat_percentage <= 0)
    bat_percentage = 1;
  Serial.print("Analog Value = " + String(sensorValue) + "\t Output Voltage = " + String(voltage) + "\t Battery Percentage = " + String(bat_percentage));
  return bat_percentage;
}

float readSonarSensor(int TRIGGER, int ECHO, String Name) {
  float distance = 0;
  byte counter = 0;
  while (distance == 0) {
    digitalWrite(TRIGGER, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER, HIGH);
    delayMicroseconds(20);
    digitalWrite(TRIGGER, LOW);
    float duration = pulseIn(ECHO, HIGH);
    distance = duration * 0.017;    //Distance read in cm
    distance = distance * 0.393701; //Distance read in inches
    delay(10);
    Serial.println(Name + " readSonarSensor distance: " + String(distance));
    counter++;
    if (counter >= 10) {
      Serial.println(Name + " readSonarSensor sensor fail and value: " + String(distance));
      break;
    }
  }
  return distance;
}

float Distance(int TRIGGER, int ECHO, String Name) {
  float distance;
  int distance1 = -1, distance2 = -1, distance3 = -1;
  bool flag = false;
  byte count = 0;
  while (flag == false) {
    distance = readSonarSensor(TRIGGER, ECHO, Name);
    if (distance1 == -1) {
      distance1 = distance;
      Serial.println(Name + " distance1 : " + String(distance1));
      count++;
    } else if (distance2 == -1) {
      distance2 = distance;
      Serial.println(Name + " distance2 : " + String(distance2));
      count++;
    } else if (distance3 == -1) {
      distance3 = distance;
      Serial.println(Name + " distance3 : " + String(distance3));
      count++;
    }
    if (distance1 == (int)distance && distance2 == (int)distance && distance3 == (int)distance) {
      Serial.println(Name + " Raw Distance : " + String(distance));
      flag = true;
      distance1 = -1, distance2 = -1, distance3 = -1;
    } else if (count >= 3) {
      Serial.println(Name + " distance : " + String(distance));
      distance1 = -1, distance2 = -1, distance3 = -1; count = 0;
    }
    Serial.println(Name + " c distance : " + String(distance));
    Serial.println(Name + " count : " + String(count) + "\n");
    delay(150);
  }
  return distance;
}

int calculatePercentageInt(float SensorDistance, float ActualTankSize) {
  int TankInPercent;
  float TankInPercent2 = 100 - ((SensorDistance / ActualTankSize) * 100);
  Serial.println("Tank is " + String(TankInPercent2) + " % Without");
  TankInPercent2 = round(TankInPercent2);
  TankInPercent = (int)TankInPercent2;

  Serial.println("Tank is " + String(TankInPercent) + " %");
  return TankInPercent;
}

// HTTP Requests Start
String loginToServer(String user, String pass) {
  String token, PostPacket, ServerResponseValue;
  PostPacket = "{\"_id\":\"" + user + "\", \"authToken\":\"" + pass + "\"}";
  int respCode = HttpRequest.Post(&ServerResponseValue, ServerHost, SigninUrl, PostPacket);
  Serial.println("\nrespCode: " + String(respCode));
  if (respCode == 200) {
    Serial.println("Successfully Login to Server:)");
    token = JsonParse.extractValue(ServerResponseValue, "token");
    Serial.println("token: " + token);
  }
  return token;
}

void versionCheck() {
  Serial.println("\nGET Request for Version");
  int respCode = HttpRequest.Get(&ServerResponseValue, ServerHost, HubUrl + HubId, AuthenticationToken);
  Serial.println("\nrespCode: " + String(respCode));
  Serial.println("ServerResponseValue: " + ServerResponseValue);
  if (respCode == 200) {
    LevelId = JsonParse.extractValue(ServerResponseValue, "levelId");
    String GetVersion = JsonParse.extractValue(ServerResponseValue, "version");
    Serial.println("\nHub LevelId: " + LevelId);
    Serial.println("\nGet Version Value: " + GetVersion);
    Serial.println("Current Version Value: " + CodeVersion);
    String PutPacket = "{\"version\":\"" + CodeVersion + "\"}";
    if (!GetVersion.startsWith("Error")) {
      if (GetVersion != CodeVersion) {
        Serial.println("\nPUT Request for Version Post");
        respCode = HttpRequest.Put(&ServerResponseValue, ServerHost, HubUrl + HubId, PutPacket, AuthenticationToken);
        if (respCode == 200)
          Serial.println("\nVersion Updated Successfully\n");
      } else Serial.println("\nVersion Updated\n");
    }
  }
}

void getConfigRequest(String configData) {
  String ConfigUrl = "/api/v1/hubs/" + HubId + "/config";
  if (configData == "") {
    int respCode = HttpRequest.Get(&ServerResponseValue, ServerHost, ConfigUrl, AuthenticationToken);
    if (respCode == 200) {
      String hubId = JsonParse.extractValue(ServerResponseValue, "hubId");
      if (hubId != "" && hubId != "Error")
        postEvt(evtTopic3, ServerResponseValue);
    }
  } else
    ServerResponseValue = configData;

  String temp = JsonParse.extractValue(ServerResponseValue, "changeBasedPost");
  if (temp != "" && temp != "Error") {
    changeBasedPost = General.StringToBool(temp);
    Serial.println("changeBasedPost : \"" + temp + "\"");
  }
  temp = JsonParse.extractValue(ServerResponseValue, "timeBasedPost");
  if (temp != "" && temp != "Error") {
    timeBasedPost = General.StringToBool(temp);
    Serial.println("timeBasedPost : \"" + temp + "\"");
  }
  temp = JsonParse.extractValue(ServerResponseValue, "readingPostTime");
  if (temp != "" && temp != "Error") {
    readingPostTime = 1000 * 60 * temp.toFloat();
    Serial.println("readingPostTime : \"" + temp + "\"");
  }
  temp = JsonParse.extractValue(ServerResponseValue, "winglePowerTime");
  if (temp != "" && temp != "Error") {
    winglePowerTime = 1000 * temp.toFloat();
    Serial.println("winglePowerTime : \"" + temp + "\"");
  }
  temp = JsonParse.extractValue(ServerResponseValue, "loopDelay");
  if (temp != "" && temp != "Error") {
    loopDelay = temp.toInt();
    Serial.println("loopDelay : \"" + temp + "\"");
    postEvt(evtTopic3, ServerResponseValue);
  }
  if (JsonParse.extractValue(ServerResponseValue, "ota") == "true") {
    String packet = "{\"ota\": false}";
    Serial.println("\nPut Request for OTA false");
    HttpRequest.Put(&ServerHost, ConfigUrl, packet, AuthenticationToken);

    OtaUrl = JsonParse.extractValue(ServerResponseValue, "otaUrl1");
    Serial.println("OTA url1 is : " + OtaUrl + "\nOTA Begin...");
    String otaresp = OtaUpdate.Update2(OtaUrl);
    postLogsEvt(evtTopic5, HubId, "", "OTA Fail due to " + otaresp);
    OtaUrl = JsonParse.extractValue(ServerResponseValue, "otaUrl2");
    Serial.println("\n\nTry with second url\nOTA url2 is : " + OtaUrl + "\nOTA Begin...");
    otaresp = OtaUpdate.Update2(OtaUrl);
    postLogsEvt(evtTopic5, HubId, "", "OTA Fail due to " + otaresp);

    // if OTA failed for some reason
    packet = "{\"ota\": true}";
    Serial.println("\nPut Request for OTA true");
    HttpRequest.Put(&ServerHost, ConfigUrl, packet, AuthenticationToken);
  }
}

// HTTP Requests End
// MQTT Start
void setMQTTParameters() {
  authMethod = OrganizationId;
  authToken = DeviceMac;
  HubId = OrganizationId + "_" + DeviceMac;
  clientName = "device-" + HubId + "-" + HubType;
  // device-LinkedThings_a848fadc9fdf-Water

  // Publish Topics
  evtTopic = "lt/evt/" + HubType + "Events/" + HubId;
  evtTopic2 = "lt/evt/VersionEvents/" + HubId;
  evtTopic3 = "lt/evt/Config/" + HubId;
  evtTopic4 = "lt/evt/Logs/" + HubId;
  evtTopic5 = "lt/evt/OtaLogs/" + HubId;
  // Subscribe Topics
  cmdTopic = "lt/cmd/AppEvents/" + HubId;

  mqttClient.setBufferSize(2048);
}

// HTTP Requests End
// MQTT Start
void mqttConnectionCheck() {
  if (!mqttClient.loop())
    mqttConnect();
}
void initialMqttConnect() {
  mqttClient.connect((char*) clientName.c_str(), (char*) authMethod.c_str(), (char*) authToken.c_str());
  initManagedDevice(cmdTopic);
}
int counter = 0, connCount = 0;
void mqttConnect() {
  if (!mqttClient.connected()) {
    digitalWrite(POWER_LED_PIN, LOW);
    Serial.println("mqttClient not connected");
    Serial.println("Reconnecting MQTT client to : " + server);
    int counter = 0;
    Serial.println("clientName: " + clientName);
    Serial.println("authMethod: " + authMethod);
    Serial.println("authToken: " + authToken);
    while (!mqttClient.connect((char*) clientName.c_str(), (char*) authMethod.c_str(), (char*) authToken.c_str())) {
      Serial.print(".");
      counter++;
      if (counter > 2) {
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("\nWiFi disconnected. Conneting again...\n");
          WIFI.APWithSTAWithoutRestart(HUB_SSID, HUB_PASSWORD, 1, WIFI_SSID, WIFI_PASSWORD, POWER_LED_PIN);
        }
        connCount++;
        if (connCount >= 5) {
          Serial.println("\nDevice Restart due to retying timeout of MQTT Client.\n");
          ESP.restart();
        }
        break;
      }
    }
    if (mqttClient.connected()) {
      digitalWrite(POWER_LED_PIN, HIGH);
      delay(1000);
    }
    initManagedDevice(cmdTopic);
  }
  if (counter != 2)
    connCount = 0;
}
void initManagedDevice(String Topic) {
  if (mqttClient.subscribe((char*) Topic.c_str())) {
    Serial.println("subscribe to " + Topic + " OK");
  }
  else {
    Serial.println("subscribe to " + Topic + " FAILED");
  }
}
void postLogsEvt(String deviceTopic, String hubId, String deviceId, String message) {
  evtPayload = "{";
  evtPayload += "\"message\": \"" + message + "\",";
  if (deviceId != "")
    evtPayload += "\"deviceId\": \"" + hubId + "_" + deviceId + "\"";
  else
    evtPayload += "\"deviceId\": \"" + hubId + "\"";
  evtPayload += "}";
  postEvt(deviceTopic, evtPayload);
}
void postEvt(String deviceTopic, String hubId, String evtType, String deviceId, String deviceValue, String reason) {
  evtPayload = "{";
  evtPayload += "\"hubId\": \"" + hubId + "\", ";
  evtPayload += "\"deviceId\": \"" + deviceId + "\", ";
  evtPayload += "\"type\": \"" + evtType + "\", ";
  evtPayload += "\"reason\": \"" + reason + "\", ";
  evtPayload += "\"value\": " + deviceValue + "";
  evtPayload += "}";
  postEvt(deviceTopic, evtPayload);
}
void postEvt(String deviceTopic, String hubId, String evtType, String deviceId, String deviceValue, String raw, String reason) {
  evtPayload = "{";
  evtPayload += "\"hubId\": \"" + hubId + "\", ";
  evtPayload += "\"deviceId\": \"" + deviceId + "\", ";
  evtPayload += "\"type\": \"" + evtType + "\", ";
  evtPayload += "\"reason\": \"" + reason + "\", ";
  evtPayload += "\"raw\": \"" + raw + "\", ";
  evtPayload += "\"value\": " + deviceValue + "";
  evtPayload += "}";
  postEvt(deviceTopic, evtPayload);
}
void postEvt(String deviceTopic, String evtPayload) {
  mqttConnect();
  Serial.println("\nTopic: " + deviceTopic);
  if (mqttClient.publish((char*) deviceTopic.c_str(), (char*) evtPayload.c_str())) {
    Serial.println("Publish ok " + evtPayload);
    digitalWrite(POWER_LED_PIN, LOW);
    delay(250);
    digitalWrite(POWER_LED_PIN, HIGH);
  } else
    Serial.println("Publish failed " + evtPayload);
}
void callback(char* topic, byte * Payload, unsigned int PayloadLength) {
  Serial.print("\ncallback invoked for topic: "); Serial.println(topic);
  for (int i = 0; i < PayloadLength; i++)
    cmdPayload += (char)Payload[i];
  if (cmdPayload.length() > 0)
    Serial.println("\nCMD receive is : " + cmdPayload);
  cmdHubId = JsonParse.extractValue(cmdPayload, "hubId");
  if (JsonParse.extractValue(cmdPayload, "restart") == "true")
    ESP.restart();
  if (HubId == cmdHubId) {
    Serial.println("Hub Id Matched CMD Accept");
    if (JsonParse.extractValue(cmdPayload, "hardReset") == "true") {
      postLogsEvt(evtTopic4, HubId, "", cmdPayload);
      EEPROMmemory.clearAtAdd(398, 399); // sIndex, eIndex
      Serial.println("\nEEPROM Cleared Successfully!");
      ESP.restart();
    } else if (JsonParse.extractValue(cmdPayload, "hardReset2") == "true") {
      postLogsEvt(evtTopic4, HubId, "", cmdPayload);
      EEPROMmemory.clear();
      Serial.println("\nEEPROM Cleared Successfully!");
      ESP.restart();
    }
    if (JsonParse.extractValue(cmdPayload, "ota") == "true") {
      OtaUrl = JsonParse.extractValue(cmdPayload, "url");
      Serial.println("OTA url is : " + OtaUrl + "\nOTA Begin...");
      String otaresp = OtaUpdate.Update2(OtaUrl);
      postLogsEvt(evtTopic5, HubId, "", "OTA Fail due to " + otaresp);
    }
    cmdDeviceId = JsonParse.extractValue(cmdPayload, "deviceId");
    cmdEvtType = JsonParse.extractValue(cmdPayload, "type");
    String temp[3];
    General.split(cmdDeviceId, 3, temp);
    cmdDeviceMac = temp[1];
    cmdDeviceName = temp[2];
    cmdDeviceValue = JsonParse.extractValue(cmdPayload, "value");
    Serial.println("\ncmdDeviceName is : " + cmdDeviceName);
    Serial.println("cmdDeviceMac is : " + cmdDeviceMac);
    Serial.println("cmdEvtType is : " + cmdEvtType);
    Serial.println("cmdDeviceValue is : " + cmdDeviceValue + "\n");
  } else
    cmdPayload = "";
}
// MQTT End
