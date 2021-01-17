//#define DEBUG_SERIAL
//#define DEBUG_TELNET
//#define DEBUG_TEMP_FILE
//#define EEPROMCLEAR // setzt EEPROM Konfiguration in Anfanszustand z.B. bei falscher fester IP
//#define DHT_SENSOR
#define BME280_SENSOR
#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdate.h>
#include <ESP32Ping.h>
#include "esp_wifi.h" // nur für fixWifiPersistencyFlag()
#include <driver/adc.h>
#include <Preferences.h>
#include "Adafruit_Sensor.h"
/*  Bei SPIFFS funktioniert nicht file.seek() für Schreiben? Jedenfall nicht mit 
    SPIFFS.open(FILENAME, "r+") getestet!
    LITTLEFS.open(FILENAME, "r+") lässt Schreiben zu ohne dass der Dateiinhalt gelöscht wird
    "w" löscht vorherigen Inhalt beim Schreiben und "a" hängt strikt neue Daten an Deiteiende */
// neu LittleFS https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
#include <esp_littlefs.h>
#include <lfs.h>
#include <lfs_util.h>
#include <LITTLEFS.h>
#include <littlefs_api.h>
#ifdef DHT_SENSOR
#include "DHT.h"    // BENÖTIGT die folgenden Arduino Bibliotheken:
//- DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
//- Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor
#endif
#ifdef BME280_SENSOR
#include <Adafruit_BME280.h>
#endif
#include <rom/rtc.h> // für rtc_get_reset_reason

#define SSID_WLAN  "Meine WLAN-SSID"     // SSID Router WLAN
#define KEY_WLAN   "Mein WLAN-Schlüssel" // WLAN Netzwerkschlüssel
#define USER_HTTP  "giessmon"
#define PASS_HTTP  "GiessMon"

#if defined DEBUG_SERIAL || defined DEBUG_TELNET
  #define DEBUG
  uint32_t debug_adr;
  int32_t debug_delAdr;
  char debugBuffer[150];
#else
  #define DEBUG_SERIAL
  char debugBuffer[150];
#endif

// WARNUNG!!! Bei Änderung dieses Schlüssels wird EEPROM mit 0x0 initialisiert und werden Standardwerte gesetzt
// Nur angelegter Variablenname wäre auch ausreichend.
#define FIRST_INIT_KEY  "tf~wU62!"

char version[] = "1.0";
String today  PROGMEM = __DATE__;
String tstamp PROGMEM = __TIME__;
String compiled_year PROGMEM = today.substring(7);

#define EEPROM_ALLOCATE_MEM 2048
#define EEPROM_WLANSSID        0 //  32
#define EEPROM_WLANKEY        32 //  32
#define EEPROM_HTTPUSER       64 //  32
#define EEPROM_HTTPPASS       96 //  32
#define EEPROM_NTPSERVER     128 //  32
#define EEPROM_OFFSETTZ      160 //  1
#define EEPROM_MANUALNETWORK 161 //  1
#define EEPROM_IPADDR        162 //  4
#define EEPROM_GWADDR        166 //  4
#define EEPROM_SNADDR        170 //  4
#define EEPROM_DNSADDR       174 //  4
#define EEPROM_PFLSENSOR1    178 //  15
#define EEPROM_PFLSENSOR2    193 //  15
#define EEPROM_PFLSENSOR3    208 //  15
#define EEPROM_PFLSENSOR4    223 //  15
#define EEPROM_PFLSENSOR5    238 //  15
#define EEPROM_PFLSENSOR6    253 //  15
#define EEPROMDATEND         268 

char wlanssid[EEPROM_WLANKEY - EEPROM_WLANSSID];
char wlankey[EEPROM_HTTPUSER - EEPROM_WLANKEY];
char http_user[EEPROM_HTTPPASS-EEPROM_HTTPUSER] = { 0 };
char http_pass[EEPROM_MANUALNETWORK-EEPROM_HTTPPASS] = { 0 };

#define MEASURE_PER_HOUR         12                    // Anzahl Messwerte pro Stunde für Mittelwert im RAM
#define POLL_INTERVALL         5000                    // für Kalibrierung aller 5 Sekunden
#define POLL_MEAN          (3600000 / MEASURE_PER_HOUR)// aller x Minuten für Berechnung gleitender Mittelwert
#define POLL_HTS               5000                    // Intervall von Temeperatur und Luftfreuchtemessungen in Millisekunden
#define ORDER 8 // Ordnung, bzw. Anzahl Elemente aus denen gewichteter Mittelwert gebildet wird;

unsigned long pollIntervall = 0; // Intervall Feuchtigkeit lesen
unsigned long pollMean = 0;      // Intervall Messwerte zur einfachen Mittelwertsberechnung
unsigned long pollHTS = 0;       // Intervall HT lesen

enum LedState {
  Dry,
  Warn,
  Humid,
  NoSensor
};

enum sstatus {
  DataValid,
  DataInvalid,
  SensorCalibrate,
  SensorNotDetected
};

#define MaxSensors 6     // Maximale Anzahl an anschließbaren FeuchteSensoren

#define INAKTIVSENSOR 0
#define RED           1
#define YELLOW        2
#define GREEN         3

typedef struct {
  int16_t temperature = 0;
  int16_t humidity = 0;
} ht_sensor_t;

typedef struct {
  uint8_t rd = 0;
  uint8_t gn = 0;
  uint8_t bl = 0;
} rgbt_t;

typedef struct {
  uint16_t  raw_dry;
  uint16_t  raw_humid;
  uint8_t   dryPercent;
  uint8_t   humidPercent;
} calibrate_sensor_t;

typedef struct {
  uint16_t Humid_RAW[MaxSensors] = {0, 0, 0, 0, 0, 0};
  uint16_t RAW_Min[MaxSensors] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
  uint16_t RAW_Max[MaxSensors] = {0, 0, 0, 0, 0, 0};
  int16_t  Percent[MaxSensors] = {0, 0, 0, 0, 0, 0};  // Feuchtigkeitssensordaten in Prozent; int16_t auch bei ungültigen Daten vernüftigen Wert anzeigen z.B. 130%
  boolean  written[MaxSensors] = {true, true, true, true, true, true}; // falls mal DataInvalid speichern für Mittelwert nochmal versuchen
  sstatus  Status [MaxSensors] = {SensorNotDetected, SensorNotDetected, SensorNotDetected, SensorNotDetected, SensorNotDetected, SensorNotDetected};
} MeasureSensorData_t;

typedef struct {
  uint16_t lastValue;
  uint16_t sumValue;
  uint8_t divisor = 0;
} mData_t;

typedef struct {
  mData_t Sensor[MaxSensors];
} MeanSensorsData_t;

MeanSensorsData_t MeanData;

typedef struct {
  int16_t sumTemperature;
  int16_t sumHumid;
  uint8_t divisor = 0;
} htMeanData_t;
htMeanData_t htMeanData;

typedef struct {
  union {
    struct {
      uint32_t writePos;
      uint16_t entries;
    };
    uint8_t ar[6]; // muss feste Größe sein! Ist sizeof(writePos) + sizeof(entries)
  };
} record_t;

//typedef struct history_sensor_t{
struct history_sensor_t{
  union {
    struct {
      uint16_t DayHour;
      int16_t temperature;
      int16_t humidity;
      int8_t  humidPercent[MaxSensors]; // nur 0 bis 100% möglich; <0 oder >100% werden in Bemerkung angezeigt
    };
    uint8_t ar[12]; // muss feste Größe sein! Ist sizeof(DayHour) + sizeof(ht_sensor_t) + sizeof(ht_sensor_t)
  };
};


const String HISTORY_FILENAME = "/history.bin";
const String BACKUP_FILENAME  = "/history.bak"; // Datensicherung
const String TEMP_FILENAME    = "/history.tmp"; // Temporäre Datei für letzten Datensatz löschen 'M'
File binFile;
#define MAXHISTORYENTRIES 552 // sind ca. 3 Wochen (23 Tage)
const uint8_t   RECORD_SIZE   = sizeof(record_t);
const uint8_t   HISTSENS_SIZE = sizeof(history_sensor_t);
const uint32_t  HISTORYEND    = HISTSENS_SIZE * MAXHISTORYENTRIES + RECORD_SIZE;

typedef struct {
  ht_sensor_t HT_Sensor;
  int8_t  humidPercent[MaxSensors]; // SensorStatus wird erst bei renewindex ermittelt
} DataToDisplay_t;

ht_sensor_t Offset;
calibrate_sensor_t CalibData;

ht_sensor_t HT_Sensor;
MeasureSensorData_t Measure;
DataToDisplay_t DataToDisplay;

typedef struct {
  float Humidity = 0 ;      // Luftfeuchtigkeitssensordaten in Prozent
  float Temperature = 0;
  sstatus  Status = SensorNotDetected;
} HTSensorData_t;

HTSensorData_t  HTMeasure;

LedState ledColors = Warn; // für radio LED-Farbe
rgbt_t  led_rgb;
uint8_t swShowRollingMean = 0;
uint8_t swSetLEDcolor = 0;
boolean showTemperatureChart = false;
boolean showHumidChart = false;
uint8_t ChartTable = 0;
boolean changeChart = false;

// EEPROM-Adressen für Kalibrierungswerte
#define NO_EEPROM 0
#define EEPR_HTS_TEMP       270 // 2 Byte
#define EEPR_HTS_HUMID      272 // 2 Byte
#define EEPR_SHOWGWM        274 // 1 Byte
#define EEPR_HYSTERESE      275 // 1 Byte
#define EEPR_LED_DRY        276 // 3 Byte
#define EEPR_LED_WARN       279 // 3 Byte
#define EEPR_LED_HUMID      282 // 3 Byte
#define EEPR_LED_NOSENSOR   285 // 3 Byte
#define EEPR_SC0_RDRY       288 // 2 Byte 
#define EEPR_SC0_RHUMID     290 // 2 Byte
#define EEPR_SC0_DRY        292 // 1 Byte
#define EEPR_SC0_HUMID      293 // 1 Byte
#define EEPR_SC1_RDRY       294 // 2 Byte
#define EEPR_SC1_RHUMID     296 // 2 Byte
#define EEPR_SC1_DRY        298 // 1 Byte
#define EEPR_SC1_HUMID      299 // 1 Byte
#define EEPR_SC2_RDRY       300 // 2 Byte
#define EEPR_SC2_RHUMID     302 // 2 Byte
#define EEPR_SC2_DRY        304 // 1 Byte
#define EEPR_SC2_HUMID      305 // 1 Byte
#define EEPR_SC3_RDRY       306 // 2 Byte
#define EEPR_SC3_RHUMID     308 // 2 Byte
#define EEPR_SC3_DRY        310 // 1 Byte
#define EEPR_SC3_HUMID      311 // 1 Byte
#define EEPR_SC4_RDRY       312 // 2 Byte
#define EEPR_SC4_RHUMID     314 // 2 Byte
#define EEPR_SC4_DRY        316 // 1 Byte
#define EEPR_SC4_HUMID      317 // 1 Byte
#define EEPR_SC5_RDRY       318 // 2 Byte
#define EEPR_SC5_RHUMID     320 // 2 Byte
#define EEPR_SC5_DRY        322 // 1 Byte
#define EEPR_SC5_HUMID      323 // 1 Byte
#define EEPR_END            324

IPAddress AppIp;
boolean wifi_failed = false;
WebServer webServer(80);
WebServer updateServer(8080);
WiFiServer telnetServer(23);
WiFiClient telnetClient;
uint8_t wifiStatus = 0;
uint16_t currentDayHour;
int8_t tzOffset;
const int   daylightOffset_sec = 3600; 
#define STARTTIME 300000
unsigned long starttime;

// Portedefinierung RGP LED Modul
#define LED_RED     0
#define LED_BLUE    14
#define LED_GREEN   15

// LED PWM Einstellungen
#define PWMfreq 5000  // 5 Khz Basisfrequenz für LED Anzeige
#define PWMledChannelA  0
#define PWMledChannelB  1
#define PWMledChannelC  2
#define PWMresolution   8 // 8 Bit Auflösung für LED PWM

//Sonstige Definitionen
#define ADCAttenuation ADC_ATTEN_DB_11    //ADC_ATTEN_DB_11 = 0-3,6V  Dämpfung ADC (ADC Erweiterung

#ifdef DHT_SENSOR
//DHT Konfiguration
#define DHTPIN 4     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE); // DHT Instanz initalisieren
#endif
#ifdef BME280_SENSOR
Adafruit_BME280 hts;
uint8_t i2caddr = 0x76;
#endif


#define MEN_INDEX  0
#define MEN_CALIB  1
#define MEN_CONFIG 2

// nachfolgend wird auf VT_..... (Value Type) angewendet
#define MASK_SHOW   0b10000000 // Kennung für Darstellung auf html-Seite
#define VT_STRING      1
#define VT_INTLIST     2
#define VT_CHECK       3
#define VT_RADIO       4
#define VT_ONOFF       5
#define VT_PORT        6
#define VT_IPNET       7
#define VT_RAW        (8 | MASK_SHOW)
#define VT_RAW_MIN    (9 | MASK_SHOW)
#define VT_RAW_MAX   (10 | MASK_SHOW)
#define VT_PROZENT   (11 | MASK_SHOW)
#define VT_INPGRD     12
#define VT_INPPRZ     13
#define VT_INP_LED_RD 14
#define VT_INP_LED_GN 15
#define VT_INP_LED_BL 16
#define VT_INP_RAW    17
#define VT_INP_TRI    18
#define VT_ROOMT      (19 | MASK_SHOW)
#define VT_ROOMH      (20 | MASK_SHOW)
#define VT_HYSTERESE  21
#define VT_MINMAXDEL  22

#define SW_GMW       200
#define SW_SHOWLED   201

#define NO_CAT       0
#define CAT_WLAN     1
#define CAT_HTTP     2
#define CAT_NTP      3
#define CAT_NET      4
#define CAT_PNAME    5

const char STR000[]  PROGMEM = "";
const char STRK28[]  PROGMEM = "RAW Temperatur";
const char STRK29[]  PROGMEM = "RAW Luftfeuchte";
const char STRK02[]  PROGMEM = "Offset Raumsensor";
const char STRK03[]  PROGMEM = "LED-Farbe";

const char STRK01[]  PROGMEM = "Sonstiges";
const char STRK04[]  PROGMEM = "Pflanzensensor 1";
const char STRK05[]  PROGMEM = "Pflanzensensor 2";
const char STRK06[]  PROGMEM = "Pflanzensensor 3";
const char STRK07[]  PROGMEM = "Pflanzensensor 4";
const char STRK08[]  PROGMEM = "Pflanzensensor 5";
const char STRK09[]  PROGMEM = "Pflanzensensor 6";

const char STRK30[]  PROGMEM = "Setze LED-Farbe";
const char STRK10[]  PROGMEM = "Gleitender MW";
const char STRK31[]  PROGMEM = "Hysterese gMW";
const char STRK32[]  PROGMEM = "Max/Min löschen";

const char STRK11[]  PROGMEM = "Temperatur in &deg;C";
const char STRK12[]  PROGMEM = "Luftfeuchte in %";

const char STRK13[]  PROGMEM = "Trocken";
const char STRK14[]  PROGMEM = "Warnung";
const char STRK15[]  PROGMEM = "Feucht";
const char STRK16[]  PROGMEM = "Kein Sensor";

const char STRK17[]  PROGMEM = "Rot";
const char STRK18[]  PROGMEM = "Grün";
const char STRK19[]  PROGMEM = "Blau";
const char STRK20[]  PROGMEM = "Feuchte RAW";
const char STRK21[]  PROGMEM = "RAW Min Nass";
const char STRK22[]  PROGMEM = "RAW Max Trock.";
const char STRK23[]  PROGMEM = "Feuchtigkeit";
const char STRK24[]  PROGMEM = "RAW Trocken";
const char STRK25[]  PROGMEM = "RAW Nass";
const char STRK26[]  PROGMEM = "Trocken in %";
const char STRK27[]  PROGMEM = "Feucht  ab %";

typedef struct {
  const char*   legend;
  uint8_t       legend_len;
  const char*   desc;
  uint8_t       desc_len;
  uint8_t       v_type;
  uint8_t       swsetX;  // Schalter anzeigen und X-Parameter setzen
  uint16_t      adrEEPR; // für Indexierung Datenablage im EEPROM
  uint8_t       val_len; // Anzahl value Bytes oder bei NO_EEPROM value für radio oder im RAM speichern bei Wert 3
} calibrate_t;

const calibrate_t calibrateTable[] PROGMEM = {
  {STRK02, sizeof(STRK02), STRK28, sizeof(STRK28),  VT_ROOMT, 0, NO_EEPROM,  0},
  {STR000, sizeof(STR000), STRK29, sizeof(STRK29),  VT_ROOMH, 0, NO_EEPROM,  0},
  {STR000, sizeof(STR000), STRK11, sizeof(STRK11),  VT_INPGRD, 1, EEPR_HTS_TEMP,  2}, // auf 2 Byte
  {STR000, sizeof(STR000), STRK12, sizeof(STRK12),  VT_INPPRZ, 1, EEPR_HTS_HUMID, 2}, // auf 2 Byte
  
  {STRK01, sizeof(STRK01), STRK30, sizeof(STRK30),  VT_ONOFF, 0, NO_EEPROM, SW_SHOWLED},
  {STR000, sizeof(STR000), STRK10, sizeof(STRK10),  VT_ONOFF, 1, EEPR_SHOWGWM, 1}, // mit Set speichern!
  {STR000, sizeof(STR000), STRK31, sizeof(STRK31),  VT_HYSTERESE, 1, EEPR_HYSTERESE, 1},
  {STR000, sizeof(STR000), STRK32, sizeof(STRK32),  VT_MINMAXDEL, 0, NO_EEPROM, 0},
  
  {STRK03, sizeof(STRK03), STRK13, sizeof(STRK13),  VT_RADIO, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK14, sizeof(STRK14),  VT_RADIO, 0, NO_EEPROM, 1},
  {STR000, sizeof(STR000), STRK15, sizeof(STRK15),  VT_RADIO, 0, NO_EEPROM, 2},
  {STR000, sizeof(STR000), STRK16, sizeof(STRK16),  VT_RADIO, 0, NO_EEPROM, 3},
  
  {STR000, sizeof(STR000), STRK17, sizeof(STRK17),  VT_INP_LED_RD, 3, NO_EEPROM, 3}, // BTNUMBER
  {STR000, sizeof(STR000), STRK18, sizeof(STRK18),  VT_INP_LED_GN, 0, NO_EEPROM, 3}, // BTNUMBER
  {STR000, sizeof(STR000), STRK19, sizeof(STRK19),  VT_INP_LED_BL, 0, NO_EEPROM, 3}, // BTNUMBER
  
  {STRK04, sizeof(STRK04), STRK20, sizeof(STRK20),  VT_RAW, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK22, sizeof(STRK22),  VT_RAW_MAX, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK21, sizeof(STRK21),  VT_RAW_MIN, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK23, sizeof(STRK23),  VT_PROZENT, 0, NO_EEPROM, 0}, // %
  {STR000, sizeof(STR000), STRK24, sizeof(STRK24),  VT_INP_RAW, 1, EEPR_SC0_RDRY, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK25, sizeof(STRK25),  VT_INP_RAW, 1, EEPR_SC0_RHUMID, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK26, sizeof(STRK26),  VT_INP_TRI, 1, EEPR_SC0_DRY, 1}, // BTNUMBER
  {STR000, sizeof(STR000), STRK27, sizeof(STRK27),  VT_INP_TRI, 1, EEPR_SC0_HUMID, 1}, // BTNUMBER
  {STRK05, sizeof(STRK05), STRK20, sizeof(STRK20),  VT_RAW, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK22, sizeof(STRK22),  VT_RAW_MAX, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK21, sizeof(STRK21),  VT_RAW_MIN, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK23, sizeof(STRK23),  VT_PROZENT, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK24, sizeof(STRK24),  VT_INP_RAW, 1, EEPR_SC1_RDRY, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK25, sizeof(STRK25),  VT_INP_RAW, 1, EEPR_SC1_RHUMID, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK26, sizeof(STRK26),  VT_INP_TRI, 1, EEPR_SC1_DRY, 1}, // BTNUMBER
  {STR000, sizeof(STR000), STRK27, sizeof(STRK27),  VT_INP_TRI, 1, EEPR_SC1_HUMID, 1}, // BTNUMBER
  {STRK06, sizeof(STRK06), STRK20, sizeof(STRK20),  VT_RAW, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK22, sizeof(STRK22),  VT_RAW_MAX, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK21, sizeof(STRK21),  VT_RAW_MIN, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK23, sizeof(STRK23),  VT_PROZENT, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK24, sizeof(STRK24),  VT_INP_RAW, 1, EEPR_SC2_RDRY, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK25, sizeof(STRK25),  VT_INP_RAW, 1, EEPR_SC2_RHUMID, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK26, sizeof(STRK26),  VT_INP_TRI, 1, EEPR_SC2_DRY, 1}, // BTNUMBER
  {STR000, sizeof(STR000), STRK27, sizeof(STRK27),  VT_INP_TRI, 1, EEPR_SC2_HUMID, 1}, // BTNUMBER
  {STRK07, sizeof(STRK07), STRK20, sizeof(STRK20),  VT_RAW, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK22, sizeof(STRK22),  VT_RAW_MAX, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK21, sizeof(STRK21),  VT_RAW_MIN, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK23, sizeof(STRK23),  VT_PROZENT, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK24, sizeof(STRK24),  VT_INP_RAW, 1, EEPR_SC3_RDRY, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK25, sizeof(STRK25),  VT_INP_RAW, 1, EEPR_SC3_RHUMID, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK26, sizeof(STRK26),  VT_INP_TRI, 1, EEPR_SC3_DRY, 1}, // BTNUMBER
  {STR000, sizeof(STR000), STRK27, sizeof(STRK27),  VT_INP_TRI, 1, EEPR_SC3_HUMID, 1}, // BTNUMBER
  {STRK08, sizeof(STRK08), STRK20, sizeof(STRK20),  VT_RAW, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK22, sizeof(STRK22),  VT_RAW_MAX, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK21, sizeof(STRK21),  VT_RAW_MIN, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK23, sizeof(STRK23),  VT_PROZENT, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK24, sizeof(STRK24),  VT_INP_RAW, 1, EEPR_SC4_RDRY, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK25, sizeof(STRK25),  VT_INP_RAW, 1, EEPR_SC4_RHUMID, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK26, sizeof(STRK26),  VT_INP_TRI, 1, EEPR_SC4_DRY, 1}, // BTNUMBER
  {STR000, sizeof(STR000), STRK27, sizeof(STRK27),  VT_INP_TRI, 1, EEPR_SC4_HUMID, 1}, // BTNUMBER
  {STRK09, sizeof(STRK09), STRK20, sizeof(STRK20),  VT_RAW, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK22, sizeof(STRK22),  VT_RAW_MAX, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK21, sizeof(STRK21),  VT_RAW_MIN, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK23, sizeof(STRK23),  VT_PROZENT, 0, NO_EEPROM, 0},
  {STR000, sizeof(STR000), STRK24, sizeof(STRK24),  VT_INP_RAW, 1, EEPR_SC5_RDRY, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK25, sizeof(STRK25),  VT_INP_RAW, 1, EEPR_SC5_RHUMID, 2}, // BTNUMBER
  {STR000, sizeof(STR000), STRK26, sizeof(STRK26),  VT_INP_TRI, 1, EEPR_SC5_DRY, 1}, // BTNUMBER
  {STR000, sizeof(STR000), STRK27, sizeof(STRK27),  VT_INP_TRI, 1, EEPR_SC5_HUMID, 1} // BTNUMBER
};

const uint8_t CALIBRATE_TABLE_LINES = (*(&calibrateTable + 1) - calibrateTable);


// Legende Block
const char STRC01[] PROGMEM = "WiFi";
const char STRC02[] PROGMEM = "HTTP";
const char STRC04[] PROGMEM = "Netzwerkzeit (NTP)";
const char STRC07[] PROGMEM = "pushbullet Benachrichtigung";
const char STRC12[] PROGMEM = "Manuelle IP-Einstellungen";
const char STRC13[] PROGMEM = "Pflanzenname";
const char STRC20[] PROGMEM = "SSID:";
const char STRC21[] PROGMEM = "Kennwort:";
const char STRC22[] PROGMEM = "Benutzername:";
const char STRC24[] PROGMEM = "Server:";
const char STRC25[] PROGMEM = "Offset Zeitzone:";
const char STRC26[] PROGMEM = "Aktiviert";
const char STRC37[] PROGMEM = "IP-Addresse:";
const char STRC38[] PROGMEM = "Gateway:";
const char STRC39[] PROGMEM = "Netzmaske:";
const char STRC40[] PROGMEM = "DNS-Server:";
const char STRC41[] PROGMEM = "Neu verbinden:";

typedef struct {
  const char*   legend;
  uint8_t       legend_len;
  const char*   desc;
  uint8_t       desc_len;
  uint8_t       v_type;    // Datentyp Zeit, Schalter, Auswahlliste, bestimmt)
  uint8_t       category;
  uint8_t       swsetX;    // Schalter anzeigen und X-Parameter setzen
  unsigned int  idxEEPROM; // für Indexierung Datenablage im EEPROM
  uint8_t       maxlen;    // Längenbegrenzung Text handle_config auf Webseite
} config_t;

const config_t configTable[] PROGMEM = {
  {STRC01, sizeof(STRC01), STRC20, sizeof(STRC20), VT_STRING, CAT_WLAN, 2, EEPROM_WLANSSID, EEPROM_WLANKEY-EEPROM_WLANSSID-1}, // -1 => Null terminierter String/char*
  {STR000, sizeof(STR000), STRC21, sizeof(STRC21), VT_STRING, NO_CAT,   0, EEPROM_WLANKEY, EEPROM_HTTPUSER-EEPROM_WLANKEY-1},
  {STRC02, sizeof(STRC02), STRC22, sizeof(STRC22), VT_STRING, CAT_HTTP, 2, EEPROM_HTTPUSER, EEPROM_HTTPPASS-EEPROM_HTTPUSER-1},
  {STR000, sizeof(STR000), STRC21, sizeof(STRC21), VT_STRING, NO_CAT,   0, EEPROM_HTTPPASS, EEPROM_NTPSERVER-EEPROM_HTTPPASS-1},
  {STRC04, sizeof(STRC04), STRC24, sizeof(STRC24), VT_STRING, CAT_NTP,  1, EEPROM_NTPSERVER, EEPROM_OFFSETTZ-EEPROM_NTPSERVER-1},
  {STR000, sizeof(STR000), STRC25, sizeof(STRC25), VT_INTLIST,NO_CAT,   1, EEPROM_OFFSETTZ, 0},
  {STRC12, sizeof(STRC12), STRC26, sizeof(STRC26), VT_CHECK , CAT_NET,  5, EEPROM_MANUALNETWORK, 0},
  {STR000, sizeof(STR000), STRC37, sizeof(STRC37), VT_IPNET , NO_CAT,   0, EEPROM_IPADDR, 15},
  {STR000, sizeof(STR000), STRC38, sizeof(STRC38), VT_IPNET , NO_CAT,   0, EEPROM_GWADDR, 15},
  {STR000, sizeof(STR000), STRC39, sizeof(STRC39), VT_IPNET , NO_CAT,   0, EEPROM_SNADDR, 15},
  {STR000, sizeof(STR000), STRC40, sizeof(STRC40), VT_IPNET , NO_CAT,   0, EEPROM_DNSADDR, 15},

  {STRC13, sizeof(STRC13), STRK04, sizeof(STRK04), VT_STRING , CAT_PNAME, 1, EEPROM_PFLSENSOR1, 14},
  {STR000, sizeof(STR000), STRK05, sizeof(STRK05), VT_STRING , CAT_PNAME, 1, EEPROM_PFLSENSOR2, 14},
  {STR000, sizeof(STR000), STRK06, sizeof(STRK06), VT_STRING , CAT_PNAME, 1, EEPROM_PFLSENSOR3, 14},
  {STR000, sizeof(STR000), STRK07, sizeof(STRK07), VT_STRING , CAT_PNAME, 1, EEPROM_PFLSENSOR4, 14},
  {STR000, sizeof(STR000), STRK08, sizeof(STRK08), VT_STRING , CAT_PNAME, 1, EEPROM_PFLSENSOR5, 14},
  {STR000, sizeof(STR000), STRK09, sizeof(STRK09), VT_STRING , CAT_PNAME, 1, EEPROM_PFLSENSOR6, 14}
};

const uint8_t CONFIG_TABLE_LINES = (*(&configTable + 1) - configTable);
