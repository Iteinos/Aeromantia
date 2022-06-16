#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DFRobot_Aliyun.h"
#include <Adafruit_AHT10.h>
Adafruit_AHT10 aht;
#include <Wire.h>
#include "Adafruit_SGP30.h"
Adafruit_SGP30 sgp;
/*配置WIFI名和密码*/
const char * WIFI_SSID     = "Apocope"; //Your WIFI
const char * WIFI_PASSWORD = "InAlterisVitis"; //Your credentials

/*配置设备证书信息*/
String ProductKey = "gsegm******"; //Change to yours
String ClientId = "10005";
String DeviceName = "Apo_Aeromantia_1";
String DeviceSecret = "23cda922cabe**************832bf9"; //Change to yours
/*配置域名和端口号*/
String ALIYUN_SERVER = "iot-as-mqtt.cn-shanghai.aliyuncs.com";
uint16_t PORT = 1883;

/*需要操作的产品标识符(温度和湿度两个标识符)*/
String TempIdentifier = "you Temp Identifier";
String HumiIdentifier = "you Humi Identifier";

/*需要上报和订阅的两个TOPIC*/
const char * subTopic = "/sys/gsegmA5BdwK/Apo_Aeromantia_1/thing/event/property/post_reply";//****set
const char * pubTopic = "/sys/gsegmA5BdwK/Apo_Aeromantia_1/thing/event/property/post";//******post
String TemId = "Temperatura";
String HumId = "Humiditas";
String DenId = "Densitas_Compositi_Organici_Volatilis_Totalis";
DFRobot_Aliyun myAliyun;
WiFiClient espClient;
PubSubClient client(espClient);
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}
void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void callback(char * topic, byte * payload, unsigned int len) {
  Serial.print("Recu [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < len; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void ConnectAliyun() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    /*根据自动计算的用户名和密码连接到Alinyun的设备，不需要更改*/
    if (client.connect(myAliyun.client_id, myAliyun.username, myAliyun.password)) {
      Serial.println("connected");
      client.subscribe(subTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
int counter = 0;
void setup() {
  Serial.begin(115200);
  aht.begin();
  sgp.begin();
  /*连接WIFI*/
  connectWiFi();

  /*初始化Alinyun的配置，可自动计算用户名和密码*/
  myAliyun.init(ALIYUN_SERVER, ProductKey, ClientId, DeviceName, DeviceSecret);

  client.setServer(myAliyun.mqtt_server, PORT);

  /*设置回调函数，当收到订阅信息时会执行回调函数*/
  client.setCallback(callback);

  /*连接到Aliyun*/
  ConnectAliyun();
}

uint8_t tempTime = 0;
void loop() {
  if (!client.connected()) {
    ConnectAliyun();
  }
  /*一分钟上报两次温湿度信息*/
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

  if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }
  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");
  counter++;
  if (counter == 30) {
    counter = 0;

    uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
  }

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
  sgp.setHumidity(getAbsoluteHumidity(temp.temperature, humidity.relative_humidity));

  bool metteaj = 0;
  if (tempTime > 6) {
    tempTime = 0;
    /*DHT.read(DHT11_PIN);
      Serial.print("DHT.temperature=");
      Serial.println(DHT.temperature);
      Serial.print("DHT.humidity");
      Serial.println(DHT.humidity);*/
    if (
      client.publish(pubTopic, ("{\"id\":" + ClientId + ",\"params\":{\"" + TemId + "\":" + temp.temperature + "},\"method\":\"thing.event.property.post\"}").c_str()) &&
      client.publish(pubTopic, ("{\"id\":" + ClientId + ",\"params\":{\"" + HumId + "\":" + humidity.relative_humidity + "},\"method\":\"thing.event.property.post\"}").c_str()) &&
      client.publish(pubTopic, ("{\"id\":" + ClientId + ",\"params\":{\"" + DenId + "\":" + sgp.TVOC + "},\"method\":\"thing.event.property.post\"}").c_str()) &&
      client.publish(pubTopic, ("{\"id\":" + ClientId + ",\"params\":{\"" + "ethanol" + "\":" + sgp.rawEthanol + "},\"method\":\"thing.event.property.post\"}").c_str()) &&
      client.publish(pubTopic, ("{\"id\":" + ClientId + ",\"params\":{\"" + "hydrogenium" + "\":" + sgp.rawH2 + "},\"method\":\"thing.event.property.post\"}").c_str())
    ) {
      metteaj = 1;
    }
    if (metteaj) {
      Serial.println("Actualisation accompli.");
    } else {
      Serial.println("!Actualisation infructueux!");
    }
  } else {
    tempTime++;
    delay(500);
  }
  client.loop();
}
