//list IMEI all location
// MKG 1311703880001  IP allocation :192, 168, 0 ,101 - 192, 168, 0 ,105
// PIK 1691103880001  IP allocation :192, 168, 0 ,111 - 192, 168, 0 ,115
// LMK  1219103880001-6  IP allocation :192, 168, 0 ,121 - 192, 168, 0 ,126
// LMP  1219203880001-6  IP allocation :192, 168, 0 ,131 - 192, 168, 0 ,136
// test 566202005000002-5

#include <WiFi.h>
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

//dari code sebelumnya
#include <ArduinoJson.h>
#include "RTClib.h"
#include "time.h"
#include <HTTPClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

WebServer server(80);

DynamicJsonDocument doc(1024);
int mqtt_flag = 0;
int restart_flag = 0;//flag restart langsung dari server
int mqttconnectflag = 0;//flag succes connect mqtt
int close_time = 300000;

const char* host = "566202005000001";
#define WIFI_SSID "cariparkir"
#define WIFI_PASSWORD "LockeyV1P"
String stat_send = "" ;
String get_addr = "https://hub.api.myveego.id/lockey/getStatus/566202005000001";
String post_addr = "https://hub.api.myveego.id/lockey/postStatus/";
long rssi = 0;

//#define MQTT_HOST IPAddress(52, 187, 181, 67) //host yang salah
#define MQTT_HOST IPAddress(52, 187, 181, 132)  //host yang benar 
#define MQTT_PORT 1881    //port yang benar
//#define MQTT_PORT 1886 //port yang salah 

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

//inisialisasi dari code sebelumnya
int boot_trigger = 0;
int tone_flag = 0;
int status_lockey;
String time_detail = "";
String booking_command = "1";
int command_flag = 0;
unsigned long send_interval  = 300000;//publish interval debugging
unsigned long previousTime = 0;//millis buffer
int restartespflag = 0;
//time local & NTP config
const char* ntpServer = "id.pool.ntp.org";
const long  gmtOffset_sec = 3600 * 8; //penyebab rtc kecepatan sejam
const int   daylightOffset_sec = 0;
String time_to_pub = "";
int day_now, month_now, year_now, hour_now, min_now, sec_now;
bool opening_flag = 1;
// ir sensor pin
#define lm1 12
#define lm2 13
#define lm3 15
int lm1read, lm2read, lm3read = 0;

//battery voltage buffer
float percen_vbat, percen_buffer_vbat ;
float voltage_vbat = 0;

//usage calculate
unsigned int start_time = 0;
unsigned int end_time = 0;
unsigned int parking_duration = 0;

// motor pin
#define r0 18
#define r1 19
#define buzzer 17

//analog read battery
#define voltage 34

//ultrasonic pin
#define trigger 14
#define echo 35
int obstacle = 0;
int car_distance = 0;
int previousTime2 = 0;
bool flag_time = 0;
//deepsleep config
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP_LONG  10800        /* Time ESP32 will go to sleep (in seconds) */
#define TIME_TO_SLEEP_SHORT  10        /* Time ESP32 will go to sleep (in seconds) */

//variable untuk HTTP x MQTT
int post_flag = 0;

RTC_DATA_ATTR int bootCount = 0;
RTC_DS1307 rtc;

String state = "1"; //string state to send

//production code
String imei = "566202005000001";
String firmware_info = "v02_10_2021";

//fitur di dalam code ini
/* connection: mqtt x http
   Lockey info: bisa cek lockey info dari ip address (harus cek ip address dulu karena dhcp)
   Handle booking: kalo lockey restart tapi ada mobil diatas  tetep deteksi ada booking
   Wifi connect flag: buat restart esp kalo ga connect wifi
   Mqtt connect flag: restart esp kalo ga connect mqtt
   jam nya GMT + 8
   ditambahkan alert restart yang dikirim menggunakan http, MQTT_flag == 0
   buzzer off 
   deepsleep 
*/

//MKG
// String latitude = "-6.21743528" ;
// String longitude = "106.66364";

// PIK
// String latitude = "-6.108708" ;
// String longitude = "106.740394";

// LMK
String latitude = "-6.2614294332149" ;
String longitude = "106.81230977614";

// LMP
// String latitude = "-6.1877" ;
// String longitude = "106.7392";

//addres EEPROM
int status_lockey_addr = 9;

String booking = "0";

//variabel buzzer off
int flag_buzzer = 1;

//variabel wifi connection
int wifiConnectFlag = 0;

//variabel MQTT connection
int mqttConnectFlag = 0;


//web OTA
/* Style */
String style =
  "<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
  "input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
  "form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex =
  "<form name=loginForm>"
  "<h1>566202005000001</h1>"
  "<h2>v02_10_2021</>"
  "<input name=userid placeholder='User ID'> "
  "<input name=pwd placeholder=Password type=Password> "
  "<input type=submit onclick=check(this.form) class=btn value=Login></form>"
  "<script>"
  "function check(form) {"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{window.open('/serverIndex')}"
  "else"
  "{alert('Error Password or Username')}"
  "}"
  "</script>" + style;

/* Server Index Page */
String serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
  "<label id='file-input' for='file'>   Choose file...</label>"
  "<input type='submit' class=btn value='Update'>"
  "<br><br>"
  "<div id='prg'></div>"
  "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
  "<script>"
  "function sub(obj){"
  "var fileName = obj.value.split('\\\\');"
  "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
  "};"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  "$.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "$('#bar').css('width',Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!') "
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>" + style;

void post_request(String datapost) {
  String post_to = post_addr + datapost;
  Serial.println(post_to);
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(post_to.c_str());
    http.addHeader("Authorization", "JWT eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6MzYsIm5vZGUiOiIzNyw0MCw0MSw0Myw0NCw0NSw0Niw0Nyw0OCw0OSw1MCw1MSw1Miw1Myw1NCw1NSw1Niw1Nyw1OCw1OSw2MCw2MSw2Miw2Myw2NCw2NSw2Niw2NywxMDMsMTA0LDEwNiwxMDcifQ.C6OyunKwtMl-KI9ilx47Ex2Lj-kJdV5twojLfk74KmU");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("wifi not connected");
    //setup_wifi();

  }
}

void get_request(String get_addr) {
  HTTPClient http;
  http.begin(get_addr.c_str());
  http.addHeader("Authorization", "JWT eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6MzYsIm5vZGUiOiIzNyw0MCw0MSw0Myw0NCw0NSw0Niw0Nyw0OCw0OSw1MCw1MSw1Miw1Myw1NCw1NSw1Niw1Nyw1OCw1OSw2MCw2MSw2Miw2Myw2NCw2NSw2Niw2NywxMDMsMTA0LDEwNiwxMDcifQ.C6OyunKwtMl-KI9ilx47Ex2Lj-kJdV5twojLfk74KmU");

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    Serial.println("process the data");
    deserializeJson(doc, payload);
    JsonObject obj = doc.as<JsonObject>();
    String command = obj[String("lockey_status")];
    booking_command = command;
    String val_long = obj["lng"];
    String val_lat = obj["lat"];
    int delay_from_user = obj["delay"];
    Serial.print("delay from myveego: ");
    Serial.println(delay_from_user);
    int restart_action = obj["restart"];
    if (booking_command == "0") {
      opening();
      state = "0";
      command_flag = 1;
      booking_command = "1";
      flag_time = 1;
    }
    if (restart_action == 1) {
      //disini fill step buat ngenormalin restart
      restart_flag = 1;
    } else {
      Serial.println("no command restart");
      restart_flag = 0;
    }

    if (delay_from_user == close_time) {
      Serial.println("is same");
    } else {
      close_time = delay_from_user;
    }

  } else {
    Serial.print("error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}


void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiConnectFlag = wifiConnectFlag + 1;
  Serial.print("Attempts connecting to Wi-Fi: ");
  Serial.println(wifiConnectFlag);
  if (wifiConnectFlag == 5) {
    wifiConnectFlag = 0;
    ESP.restart();
  }
  //  IPAddress local_IP(192, 168, 0, 111);
  //  IPAddress gateway(192, 168, 0, 1);
  //  IPAddress subnet(255, 255, 255, 0);
  //  IPAddress primaryDNS(8, 8, 8, 8);   //optional
  //  IPAddress secondaryDNS(8, 8, 4, 4); //optional

  //  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  //    Serial.println("STA Failed to configure");
  //  }
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
  mqttConnectFlag = mqttConnectFlag + 1;
  Serial.print("Attempts connecting to MQTT: ");
  Serial.println(mqttConnectFlag);
  if (mqttConnectFlag == 5) {
    mqttConnectFlag = 0;
    ESP.restart();
  }
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub1 = mqttClient.subscribe("/loki_command/566202005000001", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  uint16_t packetIdSub2 = mqttClient.subscribe("/loki_setdelay/566202005000001", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  uint16_t packetIdSub3 = mqttClient.subscribe("/loki_setlatlong/566202005000001", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  mqtt_flag = 1;
  post_flag = 0;
  Serial.println("MQTT CONNECTED");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  mqtt_flag = 0;
  post_flag = 1;
  if (WiFi.isConnected()) {
    get_request(get_addr); //ini perintah http ketika mqtt ke disconnect
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String hasil = String(payload);
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  payload: ");
  Serial.println(payload);
  Serial.print("  hasil:");
  Serial.println(hasil);
  Serial.print("  total: ");
  Serial.println(total);
  String mytopic = String(topic);

  if (mytopic == "/loki_command/566202005000001") {
    if (hasil.substring(0, 4) == "open" ) {
      Serial.println("the command is opening");
      if (digitalRead(lm1) == 0 && digitalRead(lm2) == 0) {
        flag_time = 1;
        opening();
        state = "0";
        command_flag = 1;
        booking_command = "1";
        publish_data("ao");
        Serial.println("opening success...");
      }
    } else if (hasil.substring(0, 7) == "restart") {
      Serial.println("loki going to restart......");
      ESP.restart();
    } else if (hasil.substring(0, 4) == "info") {
      // String str = datatype + "," + String(databat) + "," + String(state) + "," + String(car_distance) + "," + String(!obstacle) + "," + time_detail + "," + latitude + "," + longitude + "," + String(rssi) + "," + imei ;
      const char* tosend = firmware_info.c_str();
      Serial.println(tosend);
      int16_t packetIdPub1 = mqttClient.publish("/loki_data/566202005000001", 1, true, tosend);
    } else if (hasil.substring(0, 9) == "buzzeroff") {
      flag_buzzer = 0;
      digitalWrite(buzzer, LOW);
      Serial.println("buzzer off from myVeego");
    } else if (hasil.substring(0, 8) == "buzzeron") {
      flag_buzzer = 1;
      Serial.println("buzzer ON from myVeego");
    } else if(hasil.substring(0) == "s") {
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_LONG * uS_TO_S_FACTOR);
      Serial.println("Going to sleep now");
      Serial.flush();
      esp_deep_sleep_start();
    }else {
      Serial.println("command not available");
    }
  } else if (mytopic == "/loki_setdelay/566202005000001") {
    close_time = hasil.toInt();
  } else if (mytopic == "/loki_setlatlong/566202005000001") {
    //kasih parsingan latlong
  }

}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

//dari code sebelumnya
void adjustClose() {
  digitalWrite(r0, LOW);
  digitalWrite(r1, HIGH);
}

void adjustOpen() {
  digitalWrite(r0, HIGH);
  digitalWrite(r1, LOW);
}

void stopMovement() {
  digitalWrite(r0, LOW);
  digitalWrite(r1, LOW);
}

float battery_data() {
  voltage_vbat = (3.3 * ((float)analogRead(34) / 4095));
  percen_vbat = (voltage_vbat / 2.10) * 100 ;

  if (percen_buffer_vbat <= 0 ) {
    percen_buffer_vbat = percen_vbat;
  } else if (percen_buffer_vbat > 0 && percen_vbat > percen_buffer_vbat) {
    percen_vbat = percen_buffer_vbat;
  } else {
    percen_buffer_vbat = percen_vbat;
  }

  return percen_buffer_vbat;
}

void publish_data(String datatype) {
  if (digitalRead(lm1) == 1 && digitalRead(lm2) == 1) {
    state = "0";
  }

  if (post_flag == 0) {
    obstacle =  distance();
    rssi = WiFi.RSSI();
    printLocalTime();
    DateTime time = rtc.now(); //nanti di masukin di method
    time_detail = String(time.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.println(time_detail);
    float  databat = battery_data();
    String str = datatype + "," + String(databat) + "," + String(state) + "," + String(car_distance) + "," + String(!obstacle) + "," + time_detail + "," + latitude + "," + longitude + "," + String(rssi) + "," + imei ;
    const char* tosend = str.c_str();
    Serial.println(tosend);
    int16_t packetIdPub1 = mqttClient.publish("/loki_data/566202005000001", 1, true, tosend);
    Serial.print("Publishing at QoS 1, packetId: ");
    Serial.println(packetIdPub1);

  } else {
    obstacle =  distance();
    rssi = WiFi.RSSI();
    printLocalTime();
    DateTime time = rtc.now(); //nanti di masukin di method
    time_detail = String(time.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.println(time_detail);
    float  databat = battery_data();
    String str = datatype + "," + String(databat) + "," + String(state) + "," + String(car_distance) + "," + String(!obstacle) + "," + time_detail + "," + latitude + "," + longitude + "," + String(rssi) + "," + imei ;
    post_request(str);
  }
}

void wakeup_closing(int flag_tone) {
  unsigned int breaking = 5000;
  unsigned int breaktrigger = millis();
  while (1) {
    if (flag_tone == 1 && flag_buzzer == 1) {
      Serial.println("buzzer on");
      digitalWrite(buzzer, HIGH);
    } else if (flag_tone == 0 && flag_buzzer == 1) {
      Serial.println("buzzer off");
      digitalWrite(buzzer, LOW);
    } else {
      Serial.println("buzzer off from myVeego1");
      digitalWrite(buzzer, LOW);
    }
    Serial.print("lm1read: ");
    Serial.print(digitalRead(lm1));
    Serial.print(" || ");
    Serial.print("lm2read: ");
    Serial.println(digitalRead(lm2));
    adjustClose(); // menggerakkan motor ke atas cw
    delay(5);

    if (millis() - breaktrigger >= breaking ) {
      Serial.println("back to opening");
      digitalWrite(buzzer, LOW);
      Serial.println("lokey tertahan");
      publish_data("al4");
      opening();
      state = "0";
      delay(15000);
      ESP.restart();
      break;
    }

    if (digitalRead(lm1) == 0 && digitalRead(lm2) == 0 ) {
      digitalWrite(buzzer, LOW);
      stopMovement();
      tone_flag = 0;
      break;
    }
  }
}


void overclose_closing(int flag_tone) {
  obstacle = distance();
  unsigned int breaking = 5000;
  unsigned int breaktrigger = millis();
  while (1) {
    if (flag_tone == 1 && flag_buzzer == 1) {
      Serial.println("buzzer on");
      digitalWrite(buzzer, HIGH);
    }
    else if (flag_tone == 0 && flag_buzzer == 1) {
      Serial.println("buzzer off");
      digitalWrite(buzzer, LOW);
    }
    else {
      Serial.println("buzzer off from myVeego2");
      digitalWrite(buzzer, LOW);
    }
    Serial.print("lm1read: ");
    Serial.print(digitalRead(lm1));
    Serial.print(" || ");
    Serial.print("lm2read: ");
    Serial.println(digitalRead(lm2));
    adjustOpen();
    delay(5);
    if (digitalRead(lm1) == 0 && digitalRead(lm2) == 0 ) {
      digitalWrite(buzzer, LOW);
      stopMovement();
      break;
    }
  }
}

void opening() {
  opening_flag = 0;
  digitalWrite(buzzer, LOW);
  while (1) {
    obstacle = distance();
    Serial.print("lm1read: ");
    Serial.print(digitalRead(lm1));
    Serial.print(" || ");
    Serial.print("lm2read: ");
    Serial.println(digitalRead(lm2));
    adjustOpen();
    delay(5);
    if (digitalRead(lm1) == 1 && digitalRead(lm2) == 1 ) {
      digitalWrite(buzzer, LOW);
      stopMovement();
      opening_flag = 1;
      break;

    }
  }
}

int distance() {
  digitalWrite(trigger, LOW); // Set the trigger pin to low for 2uS
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH); // Send a 10uS high to trigger ranging
  delayMicroseconds(20);
  digitalWrite(trigger, LOW); // Send pin low again
  car_distance = pulseIn(echo, HIGH, 26000); // Read in times pulse
  car_distance = car_distance / 58; //C
  if (car_distance > 100) {
    return 0;
  } else {
    return 1;
  }
}
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  } else {
    hour_now = timeinfo.tm_hour;
    min_now  = timeinfo.tm_min;
    sec_now  = timeinfo.tm_sec;

    day_now = timeinfo.tm_mday;
    month_now = timeinfo.tm_mon + 1;
    year_now = timeinfo.tm_year + 1900;
    rtc.adjust(DateTime(year_now, month_now, day_now, hour_now, min_now, sec_now));
  }
}
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  pinMode(lm1, INPUT_PULLUP);
  pinMode(lm2, INPUT_PULLUP);
  pinMode(lm3, INPUT_PULLUP);
  pinMode(r0, OUTPUT);
  pinMode(r1, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(voltage, INPUT);
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT_PULLUP);
  delay(500);
  rtc.begin();
  Serial.setTimeout(500);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }


  DateTime time = rtc.now();
  time_detail = String(time.timestamp(DateTime::TIMESTAMP_FULL));


  connectToWifi();

  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

}

void loop() {

  delay(5000);
  Serial.print("post_flag: ");
  Serial.println(post_flag);
  server.handleClient();
  if (restartespflag == 0 && mqtt_flag == 1) {
    Serial.println("Preparing MQTT");
    delay(10000); //delay 10 detik tunggu mqtt aktif
    publish_data("al3");
    restartespflag = 1;
  }
  else if (restartespflag == 0 && mqtt_flag == 0) {
    Serial.println("Preparing MQTT");
    delay(10000); //delay 10 detik tunggu mqtt aktif
    publish_data("al3");
    restartespflag = 1;
  }
  if (mqtt_flag == 1) {
    publish_data("al5");
    mqtt_flag = 0;
  }

  if (opening_flag == 1) {
    if (flag_time == 1) {
      Serial.println("waiting for parking....");
      delay(close_time);
      flag_time = 0;
    }
    else {
      obstacle = distance();

      //pengecekan saat normal
      Serial.print("lm1: ");
      Serial.println(digitalRead(lm1));
      Serial.print("lm2: ");
      Serial.println(digitalRead(lm2));
      Serial.print("obstacle: ");
      Serial.println(obstacle);
      Serial.print("command flag : ");
      Serial.println(command_flag);
      Serial.print("close time: ");
      Serial.println(close_time);

      if (!obstacle) {
        Serial.println("no car detected");
        if (digitalRead(lm1) == 1 && digitalRead(lm2) == 1) {
          if (command_flag) {
            Serial.println("hold 3 seconds before car going out");
            delay(3000);
          }
          wakeup_closing(1);
          booking_command = "1";
          state = "1";
          command_flag = 0;
          publish_data("ac");
          Serial.println("close done");

        } else if (digitalRead(lm1) == 0 && digitalRead(lm2) == 0) {
          Serial.println("normal condition");
          digitalWrite(buzzer, LOW);

        } else if (digitalRead(lm1) == 1 && digitalRead(lm2) == 0) {
          overclose_closing(1);
          state = "1";
          command_flag = 0;
          publish_data("al2");
          Serial.println("vandal close ccw done");

        } else {
          wakeup_closing(1);
          command_flag = 0;
          state = "1";
          publish_data("al");
          Serial.println("vandal close cw done");
        }

      } else {
        Serial.println("car detected");
        if (digitalRead(lm1) ==  1 && digitalRead(lm2) == 0) {
          int flag_tone = 1;
          if (flag_buzzer == 1 && flag_tone == 1) {
            digitalWrite(buzzer, HIGH);
            Serial.println("buzzer on because of vandal");
          } else if (flag_buzzer == 0 && flag_tone == 1) {
            flag_tone = 0;
            digitalWrite(buzzer, LOW);
            Serial.println("buzzer off from MyVeego3");
          }
        } else if (digitalRead(lm1) ==  0 && digitalRead(lm2) == 1) {
          int flag_tone = 1;
          if (flag_buzzer == 1 && flag_tone == 1) {
            digitalWrite(buzzer, HIGH);
            Serial.println("buzzer on because of vandal");
          } else if (flag_buzzer == 0 && flag_tone == 1) {
            flag_tone = 0;
            digitalWrite(buzzer, LOW);
            Serial.println("buzzer off from MyVeego4");
          }
        } else if ((digitalRead(lm1) && digitalRead(lm2)) && !command_flag)  {
          //    digitalWrite(buzzer, HIGH);
          //    Serial.println("buzzering on because vandal");
        } else {
          int flag_tone = 0;
          Serial.println("buzzer off");
          digitalWrite(buzzer, LOW);
        }
      }

      if (millis() - previousTime >= send_interval) { //send periodik
        publish_data("pr");
        previousTime = millis();
        Serial.println("data published");
      } else {
        Serial.println("waiting to publish");
      }
    }
  }
}
