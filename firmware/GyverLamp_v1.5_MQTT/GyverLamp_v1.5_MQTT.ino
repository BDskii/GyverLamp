
/*
  Скетч к проекту "Многофункциональный RGB светильник"
  Страница проекта (схемы, описания): https://alexgyver.ru/GyverLamp/
  Исходники на GitHub: https://github.com/AlexGyver/GyverLamp/
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver, AlexGyver Technologies, 2019
  https://AlexGyver.ru/
*/

/*
  Версия 1.4:
  - Исправлен баг при смене режимов
  - Исправлены тормоза в режиме точки доступа

  Версия 1.4 MQTT Edition:
  - Удалена настройка статического IP - статический IP лучше настраивать на стороне роутера
  - Добавлена поддержка MQTT сервера
  - Добавлено ОТА обновление через сетевой порт
  - Добавлена интеграция с Home Assistant через MQTT Discover - лампа просто появится в Home Assistant
  - Добавлена возможность выбирать цвет из RGB палитры HomeAssistant
  - Добавлено автообнаружение подключения кнопки
  - Добавлен запуск портала конфигурации при неудачном подключении к WiFi сети
  - Добавлено адаптивное подключение к MQTT серверу в случае потери соединениия
  - Добавлено принудительное включение эффекта матрицы во время OTA обновлении
  - Добавлен вывод IP адреса при пятикратном нажатии на кнопку
  - Добавлен вывод уровеня WiFi сигнала, времени непрерывной работы и причина последней перезагрузки модуля

  Версия 1.5.5
   -  Синхронизированы изменения с версией 1.5.5 оригинальной прошивки
   -  Добавлено: режим "недоступно" в HomeAssistant после  обесточивания лампы
   -  Добавлено: Управление мощностью передатчика

*/

// Ссылка для менеджера плат:
// http://arduino.esp8266.com/stable/package_esp8266com_index.json

// Для WEMOS выбираем плату LOLIN(WEMOS) D1 R2 & mini
// Для NodeMCU выбираем NodeMCU 1.0 (ESP-12E Module)

// ============= НАСТРОЙКИ =============
// -------- ВРЕМЯ -------
#define GMT 3              // смещение (москва 3)
#define NTP_ADDRESS  "europe.pool.ntp.org"    // сервер времени

// -------- РАССВЕТ -------
#define DAWN_BRIGHT 200       // макс. яркость рассвета
#define DAWN_TIMEOUT 1        // сколько рассвет светит после времени будильника, минут

// ---------- МАТРИЦА ---------
#define BRIGHTNESS 40         // стандартная маскимальная яркость (0-255)
#define CURRENT_LIMIT 2500    // лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит

#define WIDTH 16              // ширина матрицы
#define HEIGHT 16             // высота матрицы

#define COLOR_ORDER GRB       // порядок цветов на ленте. Если цвет отображается некорректно - меняйте. Начать можно с RGB

#define MATRIX_TYPE 0         // тип матрицы: 0 - зигзаг, 1 - параллельная
#define CONNECTION_ANGLE 0    // угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#define STRIP_DIRECTION 0     // направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз
// при неправильной настройке матрицы вы получите предупреждение "Wrong matrix parameters! Set to default"
// шпаргалка по настройке матрицы здесь! https://alexgyver.ru/matrix_guide/

// --------- ESP --------
#define ESP_MODE 1
// 0 - точка доступа
// 1 - локальный
byte IP_AP[] = {192, 168, 4, 100};   // статический IP точки доступа (менять только последнюю цифру)

// ----- AP (точка доступа) -------
#define AP_SSID "GyverLamp"
#define AP_PASS "12345678"
#define AP_PORT 8888
//#define WEBAUTH           // раскоментировать для базавой аутентификации на веб интерфейсе. Логин и пароль - значение переменной clientId

// ============= ДЛЯ РАЗРАБОТЧИКОВ =============
#define LED_PIN 2             // пин ленты
#define BTN_PIN 4
#define MODE_AMOUNT 18

#define NUM_LEDS WIDTH * HEIGHT
#define SEGMENTS 1            // диодов в одном "пикселе" (для создания матрицы из кусков ленты)
// ---------------- БИБЛИОТЕКИ -----------------
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define NTP_INTERVAL 600 * 1000    // обновление (10 минут)

//#define DEBUG

#include <SimpleTimer.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <GyverButton.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "fonts.h"

#define MQTT_MAX_PACKET_SIZE 1024
// ------------------- ТИПЫ --------------------

CRGB leds[NUM_LEDS];
WiFiUDP Udp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, GMT * 3600, NTP_INTERVAL);
SimpleTimer timer;
GButton touch(BTN_PIN, LOW_PULL, NORM_OPEN);
ESP8266WebServer *http; // запуск слушателя 80 порта (эйкей вебсервер)

// ----------------- ПЕРЕМЕННЫЕ ------------------

const char AP_NameChar[] = AP_SSID;
const char WiFiPassword[] = AP_PASS;
unsigned int localPort = AP_PORT;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; //buffer to hold incoming packet
String inputBuffer;
static const byte maxDim = max(WIDTH, HEIGHT);

struct ModeSettings{
  byte brightness = 50;
  byte speed = 30;
  byte scale = 40;
}; 
ModeSettings modes[MODE_AMOUNT];

byte r = 255;
byte g = 255;
byte b = 255;

struct {
  boolean state = false;
  int time = 0;
} alarm[7];

byte dawnOffsets[] = {5, 10, 15, 20, 25, 30, 40, 50, 60};
byte dawnMode;
boolean dawnFlag = false;
boolean manualOff = false;
boolean sendSettings_flag = false;

int8_t currentMode = 0;
boolean loadingFlag = true;
boolean ONflag = false;
boolean settChanged = false;
// Конфетти, Огонь, Радуга верт., Радуга гориз., Смена цвета,
// Безумие 3D, Облака 3D, Лава 3D, Плазма 3D, Радуга 3D,
// Павлин 3D, Зебра 3D, Лес 3D, Океан 3D,

unsigned char matrixValue[8][16];
String lampIP = "";
byte hrs, mins, secs;
byte days;


WiFiClient espClient;
PubSubClient mqttclient(espClient);

// ID клиента, менять для интеграции с системами умного дома в случае необходимости
String clientId = "ESP-"+String(ESP.getChipId(), HEX);
//String clientId = "ESP-8266";

bool USE_MQTT = true; // используем  MQTT?
bool _BTN_CONNECTED = true;

struct MQTTconfig {
  char HOST[32];
  char USER[32];
  char PASSWD[32];
  char PORT[10];
};

bool shouldSaveConfig = false;

void saveConfigCallback () {
  Serial.println("should save config");
  shouldSaveConfig = true;
}

char mqtt_password[32] = "DEVS_PASSWD";
char mqtt_server[32] = "";
char mqtt_user[32] = "DEVS_USER";
char mqtt_port[10] = "1883";
byte mac[6];

ADC_MODE (ADC_VCC);

int alarmTimerID;

void setup() {

  // ЛЕНТА
  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)/*.setCorrection( TypicalLEDStrip )*/;
  FastLED.setBrightness(BRIGHTNESS);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.show();

  touch.setStepTimeout(100);
  touch.setClickTimeout(500);

  Serial.begin(115200);
  Serial.println();
  delay(1000);
  
  EEPROM.begin(512);

  // WI-FI
  if (ESP_MODE == 0) {    // режим точки доступа
    WiFi.softAPConfig(IPAddress(IP_AP[0], IP_AP[1], IP_AP[2], IP_AP[3]),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));

    WiFi.softAP(AP_NameChar, WiFiPassword);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access point Mode");
    Serial.println("AP IP address: ");
    Serial.println(myIP);
    USE_MQTT = false;

  } else {                // подключаемся к роутеру

    char esp_id[32] = "";

    Serial.print("WiFi manager...");
    sprintf(esp_id, "<br><p> Chip ID: %s </p>", clientId.c_str());
    
    WiFiManager wifiManager;

    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 32);
    WiFiManagerParameter custom_mqtt_username("user", "mqtt user", mqtt_user, 32);
    WiFiManagerParameter custom_mqtt_password("password", "mqtt_password", mqtt_password, 32);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 10);
    WiFiManagerParameter custom_text_1("<br>MQTT configuration:");
    WiFiManagerParameter custom_text_2(esp_id);
    
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.setDebugOutput(false);
    wifiManager.setConfigPortalTimeout(180);

    wifiManager.addParameter(&custom_text_1);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_username);
    wifiManager.addParameter(&custom_mqtt_password);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_text_2);

    if (!wifiManager.autoConnect()) {
      if (!wifiManager.startConfigPortal()) {
         Serial.println("failed to connect and hit timeout");
      }      
    }

    if (shouldSaveConfig) {

      strcpy(mqtt_server, custom_mqtt_server.getValue());
      strcpy(mqtt_user, custom_mqtt_username.getValue());
      strcpy(mqtt_password, custom_mqtt_password.getValue());
      strcpy(mqtt_port, custom_mqtt_port.getValue());
      
      writeMQTTConfig(mqtt_server, mqtt_user, mqtt_password, mqtt_port);
      Serial.println("MQTT configuration written");
      delay(100);
    };

    Serial.print("connected! IP address: ");
    Serial.print(WiFi.localIP());
    Serial.print(". Signal strength: ");
    Serial.print(2*(WiFi.RSSI()+100));
    Serial.println("%");

    Serial.println();
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());    

    #ifdef DEBUG    
    Serial.print("Free Heap size: ");
    Serial.print(ESP.getFreeHeap()/1024);
    Serial.println("Kb");
    #endif

    WiFi.setOutputPower(20);

    if (!MDNS.begin(clientId)) {
        Serial.println("Error setting up MDNS responder!");
    }
  
    ArduinoOTA.onStart([]() {
      Serial.println("OTA Start");
      ONflag = true;
      currentMode = 16;
      loadingFlag = true;
      FastLED.clear();
      FastLED.setBrightness(modes[currentMode].brightness);
      
    });
    
    ArduinoOTA.onEnd([]() {
      Serial.println("OTA End");  //  "Завершение OTA-апдейта"
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      effectsTick();
      Serial.printf("Progress: %u%%\n\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); 
   });
    
    ArduinoOTA.begin();
  }
  
  Udp.begin(localPort);
  Serial.printf("UDP server on port %d\n", localPort);

  // EEPROM
  
  delay(50);
  initEEPROM();


  // отправляем настройки
  sendSettings();

  timeClient.begin();
  memset(matrixValue, 0, sizeof(matrixValue));

  randomSeed(micros());

  // получаем время
  byte count = 0;
  while (count < 5) {
    if (timeClient.update()) {
      hrs = timeClient.getHours();
      mins = timeClient.getMinutes();
      secs = timeClient.getSeconds();
      days = timeClient.getDay();
      break;
    }
    count++;
    delay(500);
  }
  
  webserver();
  MDNS.addService("http", "tcp", 80);

  MQTTconfig MQTTConfig = readMQTTConfig();
  
  if ((String(MQTTConfig.HOST) == "none") || (ESP_MODE == 0) || String(MQTTConfig.HOST).length() == 0) {

    USE_MQTT = false;
    Serial.println("Использование MQTT сервера отключено.");
  }

   _BTN_CONNECTED = !digitalRead(BTN_PIN);

  #ifdef DEBUG
  _BTN_CONNECTED ? Serial.println("Обнаружена сенсорная кнопка") : Serial.println("Cенсорная кнопка не обнаружена, управление сенсорной кнопкой отключено");
  #endif

  timer.setInterval(1000, updateCurrentTime); //Каждую секунду
  timer.setInterval(1800000, timeUpdate); //синхронизация времени каждые 30 минут
  timer.setInterval(3000, checkDawn); //каждые 3 секунды проверяем рассвет
  timer.setInterval(30000, eepromTick); //каждые 30 секунды проверяем наличие настроек для сохранения
  timer.setInterval(60000, infoCallback); //Каждые 60 секунды обявляем служебную инфу в MQTT
  alarmTimerID = timer.setInterval(200, showAlarm); //Выполняет фактическую отрисовку будильника
  timer.disable(alarmTimerID);

  FastLED.clear();
  FastLED.show();

}

void loop() {

  parseUDP();
  effectsTick();
  timer.run();
  buttonTick();

  MDNS.update();
  http->handleClient();

  if (USE_MQTT && !mqttclient.connected()) MQTTreconnect();
  if (USE_MQTT) mqttclient.loop();

  ArduinoOTA.handle();
  yield();
}