/*
  Easy IoT (Wi-Fi) to Scalenics

  This sketch connects to a IoT cloud "Scalenics" (https://api.scalenics.io/console/)
  using an EnOcean Shield (TCM410J) by SiMICS and ESP-WROOM-02.

  The circuit:
  *Input PIN
    RX:EnOcean (TCM410J)
  *Output PIN
    None

  Created 12 July 2016
  by LoonaiFactory

  https://github.com/simics-co/EasyIoT-wifi-scalenics
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "ESP3Parser.h"
#include "EnOceanProfile.h"

/* Device ID */
#define SEND_DEVICE_SW1 0x00200000  // Rocker Switch (EEP F6-02-04)
#define SEND_DEVICE_CN1 0x04000000  // Contact Sensor (EEP D5-00-01)
#define SEND_DEVICE_TM1 0x04000001  // Temperature Sensor (EEP A5-02-30)
#define SEND_DEVICE_TM2 0x04000002  // Temperature Sensor (EEP A5-02-05)
#define SEND_DEVICE_OC1 0x04000003  // Occupancy Sensor (EEP A5-07-01)

#define BUFF_SIZE 15
typedef struct {
  uint32_t ID;
  uint32_t data;
} StoreDataType;
StoreDataType storeDataSet[BUFF_SIZE];

uint8_t bfWritePoint;
uint8_t bfReadPoint;
bool isDataAvailable;

// Connection info data lengths
#define MAC_ADDR_LEN    6   // Length of MAC address in bytes

// Constants
unsigned char mac_addr[MAC_ADDR_LEN];

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 0);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// WiFi
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Scalenics
#define SC_USER "YOUR_SCALENICS_ACCOUNT"
#define DEVICE_TOKEN "YOUR_DEVICE_TOKEN_HERE"
#define CLIENT_ID "enocean"
#define MQTT_SERVER "api.scalenics.io"

WiFiClient client;

// MQTT client
PubSubClient mqttClient(MQTT_SERVER, 1883, NULL, client);

static void connect(void);
static void storeData(uint8_t rorg, uint32_t ID, uint32_t data, uint8_t rssi);
static void getStoreData(void);

ESP3Parser parser(storeData);
EnOceanProfile profile;


void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(57600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  bfWritePoint = 0;
  bfReadPoint = 0;
  isDataAvailable = false;

  connect();
  parser.initialization();
}

#define TIME_DELAY 50  /* msec */
uint32_t getID, getData;
String topic;
String postData;  
char mqtt_topic[64];
char mqtt_payload[32];

char deviceID[4];
char state[5];
char temp[10];

void loop()
{
  delay(TIME_DELAY);
  
  if(isDataAvailable == true) {
    noInterrupts();	/* Disable interrupt */
    getStoreData();
    interrupts();	/* Enable interrupt */
    
    topic = DEVICE_TOKEN;
    topic += "/";
    sprintf(deviceID, "%08lX", getID);
    topic += deviceID;
    topic.toCharArray(mqtt_topic, topic.length() + 1);
    
    postData = "";
    postData += "v=";

    switch(getID) {
      case SEND_DEVICE_SW1:
        sprintf(state, "%d", profile.getSwitchStatus(EEP_F6_02_04, getData));
        postData += state;
        postData.toCharArray(mqtt_payload, postData.length() + 1);
        break;
      case SEND_DEVICE_CN1:
        sprintf(state, "%d", profile.getContact(EEP_D5_00_01, getData));
        postData += state;
        postData.toCharArray(mqtt_payload, postData.length() + 1);
        break;
      case SEND_DEVICE_TM1:
        dtostrf((double)profile.getTemperature(EEP_A5_02_30, getData), 4, 1, temp);
        postData += temp;
        postData.toCharArray(mqtt_payload, postData.length() + 1);
        break;
      case SEND_DEVICE_TM2:
        dtostrf((double)profile.getTemperature(EEP_A5_02_05, getData), 4, 1, temp);
        postData += temp;
        postData.toCharArray(mqtt_payload, postData.length() + 1);
        break; 
      case SEND_DEVICE_OC1:
        sprintf(state, "%d", profile.getPIRStatus(EEP_A5_07_01, getData));
        postData += state;
        postData.toCharArray(mqtt_payload, postData.length() + 1);
        break;
      default:
        Serial.print(F("getID = "));
        Serial.println(getID, HEX);
        break;
    }
    
    Serial.print(F("  mqtt_topic : "));
    Serial.println(mqtt_topic);
    Serial.print(F("  mqtt_payload : "));
    Serial.println(mqtt_payload);
    
    if (mqttClient.connect(deviceID, SC_USER, DEVICE_TOKEN)) {
      Serial.println(F("  Connect to MQTT server.."));
      mqttClient.publish(mqtt_topic, mqtt_payload);
      Serial.println(F("  Disconnecting MQTT server.."));
      mqttClient.disconnect();
    }
  }
}

static void connect(void)
{
  Serial.println();
  
  // give the wifi module time to boot up:
  delay(100);
  
  // read and display MAC address
  String mac = WiFi.macAddress();
  Serial.print(F("MAC Address: "));
  Serial.println(mac);
  
  // start the Ethernet connection:
  Serial.print(F("Connecting to "));
  Serial.print(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  uint8_t timeout = 60; // 60 * 500 msec = 30 sec time out
  while ((WiFi.status() != WL_CONNECTED) && timeout) {
    delay(500);
    Serial.print(F("."));
    timeout--;
  }
  Serial.println();
  
  if(WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Setting static IP"));
    WiFi.config(ip, gateway, subnet);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(F("."));
    }
    Serial.println();
  }
  
  Serial.println(F("WiFi connected"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
}

static void storeData(uint8_t rorg, uint32_t ID, uint32_t data, uint8_t rssi)
{
  switch(ID) {
    case SEND_DEVICE_SW1:
    case SEND_DEVICE_CN1:
    case SEND_DEVICE_TM1:
    case SEND_DEVICE_TM2:
    case SEND_DEVICE_OC1:

      if(((rorg == RORG_1BS) && !(data & EEP_1BS_LRN_BIT))
        || ((rorg == RORG_4BS) && !(data & EEP_4BS_LRN_BIT))) {
        break;
      }

      storeDataSet[bfWritePoint].ID = ID;
      storeDataSet[bfWritePoint].data = data;
      bfWritePoint = ((++bfWritePoint) % BUFF_SIZE);

      if(bfWritePoint == bfReadPoint) {  /* Buffer overflow */
        Serial.print(F(" Buffer Overflow! : wp = rp = "));
        Serial.println(bfWritePoint, DEC);

        if(bfWritePoint % BUFF_SIZE) {
          bfWritePoint--;
        } else {
          bfWritePoint = BUFF_SIZE - 1;
        }
      }
      isDataAvailable = true;
      break;

    default:
      break;
  }
}

static void getStoreData(void)
{
  getID = storeDataSet[bfReadPoint].ID;
  getData = storeDataSet[bfReadPoint].data;
  bfReadPoint = ((++bfReadPoint) % BUFF_SIZE);

  if(bfWritePoint == bfReadPoint) {
    isDataAvailable = false;
  }
}
