#include ".\giessmon.h"

boolean Summertime_active (int year, int month, int day, int hour) {
  // ausgehend das die RTC in der Zeitzone UTC+1, also "Winterzeit Berlin" dauerhaft läuft
  // European Daylight Savings Time calculation by "jurs" for German Arduino Forum
  // input parameters: "normal time" for year, month, day, hour
  // return value: returns true during Daylight Saving Time, false otherwise
  //int x1,x2,x3;
  static int x1,x2, lastYear; // Zur Beschleunigung des Codes ein Cache für einige statische Variablen
  int x3;
  if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
  if (month>3 && month<10) return true;  // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
  // der nachfolgende Code wird nur für Monat 3 und 10 ausgeführt
  // Umstellung erfolgt auf Stunde utc_hour=1, in der Zeitzone Berlin entsprechend 2 Uhr MEZ
  // Umstellungsbeginn und -ende
  if (year!= lastYear) {
    x1= 1 + 1 + 24*(31 - (5 * year /4 + 4) % 7); 
    x2= 1 + 1 + 24*(31 - (5 * year /4 + 1) % 7);
    lastYear=year;
  }
  x3= hour + 24 * day;
  if ((month==3 && x3>=x1) || (month==10 && x3<x2)) return true; else return false;
}

int DayOfWeek(int year, uint8_t month, uint8_t day) {
  // https://www.java-forum.org/thema/wochentag-eines-datums-berechnen.102915/
  int wDay;
  uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  wDay = (year - 1900) * 365 + (year - 1900) / 4;
  if (year % 4 && month <= 2) wDay--;
  for(int i = 0; i < month - 1; i++) {
            wDay += daysInMonth[i];
  }

	return (day + wDay) % 7;
}

void UTCp1 (int &year, int &month, int &day, int &wday, int &hour, int8_t Offset) {
  // Zeitkorrektur könnte vereinfacht werden, da Mäher sicherlich vom Dez. bis Febr. nicht im Einsatz
  // Diese Funktion ist eventuell auch durch eine aus <TimeLib.h> ersetzbar
  const uint8_t numonth[] = {4, 6, 9, 11}; // Monate mit 30 Tagen
  uint8_t DaysOfMonth = 31;

  int h = hour + 24 + Offset;
  if (h < 24) {
    day--;
    wday--;
    if (wday < 0) wday = 6;
    hour = h;
  } else {
    hour = h - 24;
  }
  if (hour >=24) {
    hour -= 24;
    day++;
    wday++;
    if (wday > 6) wday = 0;
  }
  if (day == 0) month--;
  if (month == 2) { // Februar Schaltjahr?
    DaysOfMonth = 28;
    if ((year % 400) == 0 || ((year % 100) != 0 && (year % 4) == 0)) DaysOfMonth = 29;
  } else {
    for (uint8_t i = 0; i < 4; i++) {
      if (month == numonth[i]) {
        DaysOfMonth = 30;
        break;
      }
    }
  }
  if (day == 0) { // Jahreswechsel zum Januar
    day = DaysOfMonth;
    if (month == 0) {
      month = 12;
      year--;
    }
  }
  if (day > DaysOfMonth) { // Monatswechsel
    day = 1;
    month++;
    if (month > 12) { // Jahreswechsel zum Dezember
      month = 1;
      year++;
    }
  }
}

boolean getTime(tm &timeinfo) {
  boolean ret = false;
  if(getLocalTime(&timeinfo)) {
    timeinfo.tm_year += 1900;
    timeinfo.tm_mon  += 1;
    if (Summertime_active(timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday, timeinfo.tm_hour)) {
      UTCp1(timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday, timeinfo.tm_wday, timeinfo.tm_hour, 1); // Sommerzeitkorrektur   
    }
    ret = true;
  }
  return ret;
}

#if defined DEBUG || defined DEBUG_TEMP_FILE
  #if defined DEBUG_SERIAL || defined DEBUG_TELNET
  void DebugOut(boolean sdate, String message) {
    if (sdate) {
      char buffer[150];
      tm timeinfo;
      if (getTime(timeinfo)) {
        sprintf(buffer, "%2d.%02d.%04d %2d:%02d:%02d ", timeinfo.tm_mday, timeinfo.tm_mon, timeinfo.tm_year, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      }
      message = (String)buffer + message;
    }
    #ifdef DEBUG_SERIAL
    Serial.print(message);
    #endif
    #ifdef DEBUG_TELNET
    telnetClient.print(message);
    #endif
  }
  
  void DebugOut(String message) {
    DebugOut(false, message);
  }

  #endif
#endif

String to_hex(uint8_t val) {
  String message = F("");
  char hex[3];
  sprintf(hex, "%02X", val);
  message.concat(hex);
  return message;
}

boolean TimeFullHour() {
  tm timeinfo;
  boolean ret = false;

  static uint16_t oldDayHour = 0;
  if (getTime(timeinfo)) {
    currentDayHour = (timeinfo.tm_wday << 10) | (timeinfo.tm_mday << 5) | timeinfo.tm_hour;
    if (oldDayHour != currentDayHour) { // Aufzeichnung für MW zur Stunde synchronisieren
      oldDayHour = currentDayHour;
      ret = true;
    }
  }
  return ret;
}

void SetLed() {
  ledcWrite(PWMledChannelA, led_rgb.rd); // Rote LED
  ledcWrite(PWMledChannelB, led_rgb.bl); // Blaue LED
  ledcWrite(PWMledChannelC, led_rgb.gn); // Gruene LED
}

void SetLedColor(LedState state) {
  if (!swSetLEDcolor) {
    ledColors = state;
    EEPROM.get(state * sizeof(rgbt_t) + EEPR_LED_DRY, led_rgb);
    SetLed();
  }
}

void clearhtMeanData() {
  htMeanData.divisor = 0;
}

ht_sensor_t gethtMeanData() {
  ht_sensor_t ht;
  
  ht.temperature = 0;
  ht.humidity = 0;
  if (htMeanData.divisor > 0) {
    ht.temperature = htMeanData.sumTemperature / htMeanData.divisor;
    ht.humidity = htMeanData.sumHumid / htMeanData.divisor;
  } else {
    ht.temperature = int16_t(HTMeasure.Temperature * 10);
    ht.humidity = int16_t(HTMeasure.Humidity * 10);
  }
  ht.temperature += Offset.temperature;
  ht.humidity += Offset.humidity;
  return ht;
}

void puthtMeanData(float Temperature, float Humid) {
  // Daten zur Bildung eines einfachen Mittelwertes aller x Minuten einer Stunde erfassen
  EEPROM.get(EEPR_HTS_TEMP, Offset);
  if (htMeanData.divisor == 0) {
    htMeanData.sumTemperature = int16_t(Temperature * 10);
    htMeanData.sumHumid = int16_t(Humid * 10) + 5; // Nachkommastelle gerundet
    htMeanData.divisor = 1;
  } else{
    htMeanData.sumTemperature += int16_t(Temperature * 10);
    htMeanData.sumHumid += (int16_t(Humid * 10) + 5); // Nachkommastelle gerundet
    htMeanData.divisor++;
  }
  #ifdef DEBUG
  sprintf(debugBuffer, "sTemeratur: %d  sFeuchtigkeit: %d  divisor: %d\r\n", htMeanData.sumTemperature, htMeanData.sumHumid, htMeanData.divisor);
  DebugOut(true, debugBuffer);
  #endif
}

void clearMeanData(uint8_t Sensor) {
  MeanData.Sensor[Sensor].divisor = 0;
}

/* uint8_t getMeanData(uint8_t Sensor) {
  if (MeanData.Sensor[Sensor].divisor > 0) {
    return MeanData.Sensor[Sensor].sumValue / MeanData.Sensor[Sensor].divisor;
  } else
    return 0;
} */

void putMeanData(uint8_t Sensor, uint16_t data) {
  // Hysterese muss wie bei handle_renewtable() berücksichtigt werden
  // Daten zur Bildung eines einfachen Mittelwertes aller x Minuten einer Stunde erfassen
  if (MeanData.Sensor[Sensor].divisor == 0 || (MeanData.Sensor[Sensor].lastValue + EEPROM.read(EEPR_HYSTERESE)) <= data) {
    MeanData.Sensor[Sensor].lastValue = data;
    MeanData.Sensor[Sensor].sumValue = data;
    MeanData.Sensor[Sensor].divisor = 1;
  } else{
    MeanData.Sensor[Sensor].lastValue = data;
    MeanData.Sensor[Sensor].sumValue += data;
    MeanData.Sensor[Sensor].divisor++;
  }
}

int ReadMoistureSensor_Raw_Val(byte Sensor) {
  // Werte der Feuchtigkeitssensoren ermitteln
  #define NUM_READS 6
  const adc1_channel_t adc_channel[MaxSensors] = {ADC1_CHANNEL_0, ADC1_CHANNEL_3, ADC1_CHANNEL_4, ADC1_CHANNEL_5, ADC1_CHANNEL_6, ADC1_CHANNEL_7};
  long sum = 0;

  adc1_config_width(ADC_WIDTH_BIT_12);   //Bereich 0-4095
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADCAttenuation);

  for (uint8_t i = 0; i < NUM_READS; i++) { // Mittelwert bilden
    sum += adc1_get_raw(adc_channel[Sensor]); //analog lesen
    delay(10);
  }
  return sum / NUM_READS;
}

void initHistoryFile() {
  // Flashspeicher für Dateien initialisieren/mounten
  record_t record;
  boolean fsOk;

  fsOk = LITTLEFS.begin();
  if (!fsOk) {  // 1. Versuch => mount geht manchmal schief
    fsOk = LITTLEFS.begin();
  }
  if (!fsOk) {  // 2. Versuch
    fsOk = LITTLEFS.begin();
  }
  if (fsOk) {
    if (!LITTLEFS.exists(HISTORY_FILENAME)) {
      record.writePos =0x0;
      record.entries = 0x0;
      binFile = LITTLEFS.open(HISTORY_FILENAME, "w");
      binFile.write(record.ar, RECORD_SIZE);
      binFile.close();
    }
  }
}

void nextRecord(record_t record, uint8_t Sensor, uint8_t Data) {
  // neue Datenzeile in Datei anlegen oder nach Erreichung max. 
  // Einträge ältester Datenzeile überschreiben (Ringspeicher)
  history_sensor_t HistorySensor;
  ht_sensor_t ht;

  ht = gethtMeanData();
  for (uint8_t i = 0; i < MaxSensors; i++) {
    HistorySensor.humidPercent[i] = 0;
  }
  HistorySensor.DayHour = currentDayHour;
  HistorySensor.temperature = ht.temperature;
  HistorySensor.humidity = ht.humidity;
  HistorySensor.humidPercent[Sensor] = Data;

  if (record.entries < MAXHISTORYENTRIES) {
    record.entries++;
  }
  if (record.writePos >= HISTORYEND) {
    record.writePos = RECORD_SIZE;
  }
  binFile.seek(record.writePos, SeekSet);
  binFile.write(HistorySensor.ar, HISTSENS_SIZE);

  binFile.seek(0, SeekSet);
  binFile.write(record.ar, RECORD_SIZE);
  changeChart = true; // Webseite derzeit angezeigten Chart neu einlesen
  #ifdef DEBUG
  sprintf(debugBuffer, "SensorDataWritten %d  %d%%\r\n", Sensor + 1, Data);
  DebugOut(true, debugBuffer);
  #endif
}

void writeHistorySensor(uint8_t Sensor, uint8_t Data) {
  // Erfasste Messwerte für grafische Darstellung in Datei/Tabelle speichern
  // soll auch funktionieren, wenn zwischendurch esp32 mal vom Netz getrennt wurde
  record_t record;
  history_sensor_t HistorySensor;
  uint8_t buffer[1];

  binFile = LITTLEFS.open(HISTORY_FILENAME, "r+");
  binFile.read(record.ar, RECORD_SIZE);

  if (currentDayHour != 0) {
    if (record.writePos == 0) {
      record.writePos = RECORD_SIZE;
      nextRecord(record, Sensor, Data); // ersten Datensatz in noch leere Tabelle schreiben unabhängig zur vollen Stunde
    } else {
      binFile.seek(record.writePos, SeekSet);
      binFile.read(HistorySensor.ar, HISTSENS_SIZE);
      // u.a. verhindern, dass beim Neustart wiederholt Datensätze gleicher Zeit geschrieben werden
      if (HistorySensor.DayHour == currentDayHour) { // speichert somit in der Regel zur vollen Stunde
        if (HistorySensor.humidPercent[Sensor] == 0 && Data > 0) {
          // noch fehlende Sensoren im vorhandenen Datensatz nachtragen
          // somit können Messwerte neu zugeschalteter Sensoren jederzeit, zeitnah mit erfasst werden
          buffer[0] = Data;
          binFile.seek(-(MaxSensors - Sensor), SeekCur);
          binFile.write(buffer, 1);
          changeChart = true; // Webseite derzeit angezeigten Chart neu einlesen
          #ifdef DEBUG
          sprintf(debugBuffer, "SensorDataWritten %d  %d%%\r\n", Sensor + 1, Data);
          DebugOut(true, debugBuffer);
          #endif
        }

      } else {
        // Einträge/Adresszeiger erhöhen und ersten neuen Datensatz schreiben
        // somit können/müssen später abgefragte/zugeschaltete Sensoren nachgeschrieben werden
        record.writePos += HISTSENS_SIZE;
        nextRecord(record, Sensor, Data);
      }
    }
  }
  binFile.close();
}

void Get_Moisture_DataInPercent(int8_t Sensor, boolean putValue) {
  #define MinSensorValue 300

  int RawMoistureValue = ReadMoistureSensor_Raw_Val(Sensor);
  if ( RawMoistureValue <= MinSensorValue) { // Sensor angeschlossen?
    Measure.Status[Sensor] = SensorNotDetected;
    return;
  }
  Measure.Humid_RAW[Sensor] = RawMoistureValue;
  // RAW MIN und MAX ermitteln zwecks einfacherer Kalibrierung
  if (RawMoistureValue < Measure.RAW_Min[Sensor]) 
    Measure.RAW_Min[Sensor] = RawMoistureValue;
  if (RawMoistureValue >  Measure.RAW_Max[Sensor])
    Measure.RAW_Max[Sensor] = RawMoistureValue;

  EEPROM.get(sizeof(calibrate_sensor_t) * Sensor + EEPR_SC0_RDRY, CalibData);
  if ((CalibData.raw_dry == 0) || (CalibData.raw_humid == 0)) { // MinADC Value maxADC ADC Value
    Measure.Status[Sensor] = SensorCalibrate;
  } else  {
    Measure.Percent[Sensor] = map(RawMoistureValue, CalibData.raw_dry, CalibData.raw_humid, 0, 100);
    // auch fehlerhafte Prozentwerte speichern => Werden durch Status gekennzeichnet
    if (Measure.Percent[Sensor] > 100 || Measure.Percent[Sensor] < 0) {
      Measure.Status[Sensor] = DataInvalid;
    } else {
      Measure.Status[Sensor] = DataValid;
      Measure.RAW_Pct[Sensor] = Measure.Percent[Sensor];
      // putValue regelt x Minuten-Intervall Feuchtigkeitswert summieren für Ermittlung einfachen Mittelwert pro Stunde
      if (putValue || !Measure.written[Sensor]) { // falls mal DataInvalid speichern für Mittelwert nochmal versuchen
        putMeanData(Sensor, Measure.Percent[Sensor]);
        Measure.written[Sensor] = true;
        #ifdef DEBUG
        sprintf(debugBuffer, "Sensor: %d  Measure.Percent[Sensor]: %d  divisor: %d\r\n", Sensor+1, Measure.Percent[Sensor], MeanData.Sensor[Sensor].divisor);
        DebugOut(true, debugBuffer);
        #endif
      }

      if (MeanData.Sensor[Sensor].divisor > 0) { // aktuellen Mittelwert der Pflanzensensoren auf Webseite anzegen
        Measure.Percent[Sensor] = MeanData.Sensor[Sensor].sumValue / MeanData.Sensor[Sensor].divisor;
      } // zur vollen Stunde werden Mittelwerte gelöscht, dann bereits zugewiesenen aktuellen Wert anzeigen
      // falls vorherige Daten mal ungültig zeigt DataToDisplay letzten validen Wert an
      DataToDisplay.humidPercent[Sensor] = int8_t(Measure.Percent[Sensor]); // HumidState wird erst bei renewindex ermittelt
      writeHistorySensor(Sensor, uint8_t(Measure.Percent[Sensor])); // Daten zur Datei im Flash
    }
  }
  /*#ifdef DEBUG
  if (putValue && Measure.Status[Sensor] != DataValid) {*/
    sprintf(debugBuffer, "Sensor: %d  Measure.Percent: %d%%  Status: %d\r\n", Sensor + 1, Measure.Percent[Sensor], Measure.Status[Sensor]);
    DebugOut(true, debugBuffer);
  /*}
  #endif*/
}

void Run_MoistureSensors() { // Hauptfunktion zum Betrieb der Bodenfeuchtesensoren POLL_INTERVALL 5 Sec
  boolean red1 = false;
  boolean yellow1 = false;
  boolean green1 = false;
  boolean putForMean = false;

  if (pollMean < millis()) {
    putForMean = true; // aller (3600000 / MEASURE_PER_HOUR) Minuten zur einfachen Mittelwertberechnung
    pollMean = millis() + POLL_MEAN;
    puthtMeanData(HTMeasure.Temperature, HTMeasure.Humidity); // Mittelwerterfassung HT-Sensor
  }

  for (int i = 0; i < MaxSensors; i++) {
    if (putForMean) {
      Measure.written[i] = false; // wenn pollMean ausgelöst dann bis DataValid
    }
    Get_Moisture_DataInPercent(i, putForMean);
    switch (Measure.Status[i]) {
      case DataValid: {
        if ( Measure.Percent[i] >= CalibData.humidPercent) {
          green1 = true;
        } else 
        if ( Measure.Percent[i] > CalibData.dryPercent) {
          yellow1 = true;
        } else {
          red1 = true;
        }
        break;
      }
      #ifdef DEBUG
      case SensorCalibrate: {
        sprintf(debugBuffer, "Sensor %d nicht kalibiert. Bitte kalibrieren. Rohdatenwert:%d\r\n", i + 1, Measure.Humid_RAW[i]);
        DebugOut(true, debugBuffer);
        break;
      }
      case DataInvalid: {
        sprintf(debugBuffer, "Sensor %d Daten ungültig. Rohdatenwert:%d\r\n", i + 1,Measure.Humid_RAW[i]);
        DebugOut(true, debugBuffer);
        break;
      }
      #endif
      default: break;
    }
  }
  if (red1) {
    SetLedColor(Dry);
  } else 
  if (yellow1) {
    SetLedColor(Warn);
  }
  else
  if (green1) {
    SetLedColor(Humid);
  }
  else {
    SetLedColor(NoSensor);
  }
  //timerWrite(timer, 0); //timer rücksetzen watchdog
}

sstatus Run_HTSensor() {
  if (pollHTS < millis()) {
    pollHTS = millis() + POLL_HTS;
    EEPROM.get(EEPR_HTS_TEMP, Offset);

    HTMeasure.Humidity = hts.readHumidity();
    #ifdef DHT_SENSOR
    HTMeasure.Temperature = hts.readTemperature(false);   // Lese Temperatur in Celsius (bei Fahrenheit = true)
    #endif
    #ifdef BME280_SENSOR
    HTMeasure.Temperature = hts.readTemperature();
    #endif
    if (isnan(HTMeasure.Humidity) || isnan(HTMeasure.Temperature) ) {
      DebugOut(true, F("Lesen vom HT-Sensor fehlgeschlagen!"));
      HTMeasure.Status  = SensorNotDetected;
      return SensorNotDetected;
    }
    HTMeasure.Status  = DataValid;

    DataToDisplay.HT_Sensor.temperature = uint16_t(HTMeasure.Temperature * 10) + Offset.temperature;
    DataToDisplay.HT_Sensor.humidity = uint16_t(HTMeasure.Humidity * 10) + Offset.humidity;
  }
  return DataValid;
}

String getUptime() {
  String uptime = F("");
  long millisecs = millis();
  uint8_t secs = uint8_t((millisecs / 1000) % 60);
  uint8_t mins = uint8_t((millisecs / (1000 * 60)) % 60);
  uint8_t hours = int((millisecs / (1000 * 60 * 60)) % 24);
  uint8_t days = int((millisecs / (1000 * 60 * 60 * 24)) % 365);
  uptime.concat(String(days));
  uptime.concat(F("T "));
  uptime.concat(String(hours));
  uptime.concat(F(":"));
  if (mins < 10) {
    uptime.concat(F("0"));
  }
  uptime.concat(String(mins));
  uptime.concat(F(":"));
  if (secs < 10) {
    uptime.concat(F("0"));
  }
  uptime.concat(String(secs));
  return uptime;
} 

String getPlateName(uint8_t platenr) {
  return EEPROM.readString(EEPROM_PFLSENSOR1 + (platenr * (EEPROM_PFLSENSOR2 - EEPROM_PFLSENSOR1)));
}

String getTitlePlateName(uint8_t platenr) {
  uint8_t pf = 0;
  char str_buffer[20];
  uint8_t legend_len;
  String PlateName;
  String ret;

  PlateName = getPlateName(platenr);
  if (PlateName.length() > 0) {
    ret = PlateName;
  } else {
    for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
      if (pgm_read_byte(&calibrateTable[idx].v_type) == VT_RAW) {
        if (platenr == pf) {
          legend_len = pgm_read_byte(&calibrateTable[idx].legend_len);
          memcpy_P(str_buffer, (char*)pgm_read_dword(&(calibrateTable[idx].legend)), legend_len);
          ret = String(str_buffer);
          break;
        }
        pf++;
      }
    }
  }
  return ret;
}

void handle_freeheap() {
  String message = (F("{\"fheap\":\""));
  message.concat(String(ESP.getFreeHeap()));
  message.concat(F("\"}"));
  webServer.send(200, "application/json", message);
} // handle_freeheap

void webpageBegin() {
  if (!webServer.authenticate(http_user, http_pass)) {
    return webServer.requestAuthentication();
  }

  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "0");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");
  // u.a. Script mit fetch zur Datenübergabe an Webseite
  webServer.sendContent(F("<!DOCTYPE html>\n<html lang='de'><head><meta http-equiv='Content-Type'content='text/html; charset=UTF-8'>\n<style>\nhtml{font-family:Arial;max-width:100%;min-width:25em;background-color:white;color:black;}\n.tab{\noverflow:hidden;display: flex;\n}\n.tab button{\nbackground-color:#999;\ncolor:rgb(245,245,245);\nborder:none;\nmargin-top:0em;\ntransition:0.8s;\nborder-top-right-radius:.5em;\nborder-top-left-radius:.5em;\n}\n.tab button:hover{\nbackground-color:#12121294;\n}\n.tab button.active{\nbackground-color:#f5fdea;\ncolor:black;\nborder:3px solid #999;\nborder-bottom:0px;\n}\n.tabcontent{\ndisplay:none;\npadding:.5em .7em .5em .5em;\nbackground-color:#f5fdea;\nborder:3px solid #999;\nborder-top:0px;\nmin-width:25em;\noverflow:hidden;\n}\n.none{\ncolor:#999;\n}\n@media only screen{\n.tab button{\nwidth:34%;\nmin-width:6.2em;\nfont-size:1.4em;\n}\n}\n.cons{\ndisplay:block;\ntext-align:center;\nwidth:100%;\nmargin-top:1em;\nbackground-color:inherit;\npadding-bottom:1em;\n}iframe{\nmin-width:18em;\nwidth:40%;\nheight:10em;\nbackground-color:white;\ncolor:gray;\nborder-width:1px;\nborder-style:solid;\nmargin-top:.6em;\nmargin-bottom:.8em;\n}\n#title{\nmargin-top:.9em;\nmargin-bottom:.7em;\n}\n"));
}

void webpageScript() {
  webServer.sendContent(F("</style>\n<script>\ndocument.addEventListener('DOMContentLoaded',()=>{\ndocument.querySelector('#tab0').addEventListener('click',openTab);\ndocument.querySelector('#tab1').addEventListener('click',openTab);\ndocument.querySelector('#tab2').addEventListener('click',openTab);\n});\nfunction openTab(e){\nvar a=e.target.id.charAt(3);\nlet s=['','calibrate','config'];\nwindow.open('/'+s[a],'_self');\n}\n"));
}

void webpageBody(uint8_t aktiveTab) {
  String message = "";
  message.concat(F("\n</script>\n</head>\n<body><div class='tab'><button class='button"));
  if (aktiveTab == MEN_INDEX) message.concat(F(" active"));
  message.concat(F("'id='tab0'>Startseite</button>\n<button class='button"));
  if (aktiveTab == MEN_CALIB) message.concat(F(" active"));
  message.concat(F("'id='tab1'>Kalibrieren</button>\n<button class='button"));
  if (aktiveTab == MEN_CONFIG) message.concat(F(" active"));
  message.concat(F("'id='tab2'>Konfiguration</button>\n</div>\n<div class='tabcontent'style='display:block'>\n"));
  webServer.sendContent(message);
}

void webpageEnd(uint8_t aktiveTab) {
  webServer.sendContent(F("\n<block class=cons>\n"));
  if (aktiveTab == MEN_CONFIG) {
    webServer.sendContent(F("<B>Hinweise:</B><BR><iframe name='output'id='output'>\n</iframe>\n<BR>Freier dynamischer Speicher:&nbsp<span id=frh></span>\n"));
    webServer.sendContent(F("<p>Betriebszeit: "));
    webServer.sendContent(getUptime());
    webServer.sendContent(F("</p>\n<p>Version: "));
    webServer.sendContent(version);
    webServer.sendContent(F(" / "));
    webServer.sendContent(today);
    webServer.sendContent(F(" "));
    webServer.sendContent(tstamp);
  }
  webServer.sendContent(F("</p></block>\n</div>\n</body></html>"));
}

// Startwebseite
void handle_index() {
  String message;
  char str_buffer[30];
  uint8_t legend_len;
  String legend;
  uint8_t v_type;
  uint8_t idNb = 2;
  String PlateName;

  webpageBegin(); // <style>
  webServer.sendContent(F("h1,h3{text-align:center;margin-top:-.3em;margin-bottom:.2em;}\n.grid{\ndisplay:grid;\n}\nbody{align-content:center;}\n#dht{\npadding-bottom:0.2em;margin-left:10.2em;\nmargin-bottom:.6em;\nmargin-right:2em;\nfont-size:110%;\n}\n#dht1{\nwidth:7.2em;\n}\n#dht11{\nwidth:3.5em;\n}\n#dht2{\nwidth:5.8em;\n}\n#dht21{\nwidth:2.9em;\n}\n#wifi{\ntext-align:right;\n}\n#pft{\nwidth:49.6em;\nmargin-left:1.8em;\n}\n#listpfl{\nwidth:9.2em;\n}\ninput{\nfont:inherit;\nbackground-color:inherit;\nborder:none;\n}\ninput:focus{\noutline: none;\n}\ninput:hover{\ncursor: pointer;\nbackground-color:#baf5d4c0;\n}\n#hb{\nwidth:15em;\n}\n#humid{\nwidth:3.5em;\ntext-align:right;\n}\n#desc{\npadding-left:1.5em;\n}\nprogress[value]{\n-webkit-appearance:none;\n-moz-appearance:none;\nappearance:none;\nborder:none;\nbackground-color:lightgrey;\nwidth:240px;\nheight:20px;\n}\nprogress[value]::-webkit-progress-bar{\nbackground-color:lightgrey;\n}\nprogress.red{\ncolor:red;\n}\nprogress.red[value]{\ncolor:red\n}\nprogress.red::-webkit-progress-value{\nbackground-color:red\n}\nprogress.red::-moz-progress-bar{\nbackground-color:red\n}\nprogress.yellow{\ncolor:yellow;\n}\nprogress.yellow[value]{\ncolor:yellow\n}\nprogress.yellow::-webkit-progress-value{\nbackground-color:yellow\n}\nprogress.yellow::-moz-progress-bar{\nbackground-color:yellow\n}\nprogress.green{\ncolor:green;\n}\nprogress.green[value]{\ncolor:green\n}\nprogress.green::-webkit-progress-value{\nbackground-color:green\n}\nprogress.green::-moz-progress-bar{\nbackground-color:green\n}#curve_chart{\nmax-width:100%;\nmin-width:40em;\nheight:35em\n}\n#desc{color:red;\nfont-weight:bold;\n}\n"));
  webpageScript(); // </style><script>
  webServer.sendContent(F("function tblresize(){\nloadMeanTable();\n}\ndocument.addEventListener('DOMContentLoaded',()=>{\nrenewindex(),loadMeanTable();\nwindow.addEventListener('resize',tblresize);\nwindow.setInterval(renewindex,"));
  webServer.sendContent(String(POLL_INTERVALL));
  webServer.sendContent(F(");\n});\nfunction renewindex(){\nlet data=document.querySelectorAll('data');\nlet prgs=document.querySelectorAll('progress');\nlet desc=document.querySelectorAll('#desc');\nlet inp=document.querySelectorAll('input');\nfetch('/renewindex').then((resp)=>{\nreturn resp.json();\n}).then((obj)=>{\nfor(i=0;i<"));
  webServer.sendContent(String(MaxSensors + 2));
  webServer.sendContent(F(";i++){\ndata[i].innerHTML=obj[i];\nif(i>=2){\nprgs[i-2].value=obj[i];\nlet hbar=document.querySelector('#humidbar'+i);\nif(hbar.classList.contains('red'))hbar.classList.remove('red');\nif(hbar.classList.contains('yellow'))hbar.classList.remove('yellow');\nif(hbar.classList.contains('green'))hbar.classList.remove('green');\nif(obj['hstate'+i]==1){\nhbar.classList.add('red');\n}\nelse\nif(obj['hstate'+i]==2){\nhbar.classList.add('yellow');\n}\nelse\nif(obj['hstate'+i]==3){\nhbar.classList.add('green');\n}\ndesc[i-2].innerHTML=obj['sstate'+i];\nif(obj['chtbl']==i-2){\ninp[i].style.color='green';\ninp[i].style.fontWeight='bold';\n}\nelse{\ninp[i].style.color='black';\ninp[i].style.fontWeight='normal';\n}\n}\n}\nif(obj['tempchart']==true){\ndocument.querySelector('#ht250').style.color='red';\n}else{\ndocument.querySelector('#ht250').style.color='black';\n}\nif(obj['humidchart']==true){\ndocument.querySelector('#ht251').style.color='blue';\n}else{\ndocument.querySelector('#ht251').style.color='black';\n}\ndocument.querySelector('#rssi').innerHTML=obj['rssi'];\nif(obj['chgtbl']==true){\nloadMeanTable();\n}\n})\n};\nfunction setMeanTbl(){\nlet meanTblData=new FormData();\nvar temp=arguments[0].substr(2)+\",\"+arguments.length+\"\";\nmeanTblData.append('setVal',temp);\nsend(meanTblData);\nwindow.setTimeout(\"renewindex()\",70);\n}\nfunction send(arg){\nfetch('/setrequest',{\nmethod:'post',\nbody:arg\n});\n}\n</script>\n<script type='text/javascript'loading='lazy'src='https://www.gstatic.com/charts/loader.js'></script>\n<script>\nfunction loadMeanTable(){\ngoogle.charts.load('current',{\npackages:['corechart','line']\n});\ngoogle.setOnLoadCallback(drawChart);\n}\nfunction fetchTable(myCallback){\nfetch('/renewtable').then((resp)=>{\nreturn resp.json();\n}).then((obj)=>{\nlet humidchart=obj['humidchart'];\nlet pfls=obj['pfls'];\nlet vtable=obj[0];\nmyCallback(humidchart,pfls,vtable);\n});\n}\nfunction mydrawChart(humidchart,pfls,vtable){\nvar data=google.visualization.arrayToDataTable(vtable);\nvar options={\nbackgroundColor:'#f5fdea',\ncolors:['green','red','blue'],\ntitle:pfls,\nvAxes:{\n0:{\nviewWindowMode:'explicit',\ngridlines:{color:'black'},\nformat:'##%'\n},\n1:{\nviewWindowMode:'explicit',\ngridlines:{color:'transparent'},\nformat:'##°C'\n},\n2:{\nviewWindowMode:'explicit',\ngridlines:{color:'transparent'},\nformat:'##%'\n},\n},\nseries:{\n 0:{targetAxisIndex:0},\n1:{targetAxisIndex:1},\n2:{targetAxisIndex:0},\n},\nchartArea:{left:90,top:80,width:\"91%\"},\ncurveType:'none',\nlegend:{position:'bottom'}\n};\nvar optionsF={\nbackgroundColor:'#f5fdea',\ncolors:['green','blue'],\ntitle:pfls,\nvAxes:{\n0:{\n viewWindowMode:'explicit',\ngridlines:{color:'black'},\nformat:'##%'\n},\n1:{\nviewWindowMode:'explicit',\ngridlines:{color:'transparent'},\nformat:'##%'\n},\n},\nseries:{\n 0:{targetAxisIndex:0},\n1:{targetAxisIndex:0},\n},\nchartArea:{left:90,top:80,width:\"91%\"},\ncurveType:'none',\nlegend:{ position:'bottom'}\n};\nif(humidchart==true){\noptions=optionsF;\n}\nvar chart=new google.visualization.LineChart(document.getElementById('curve_chart'));\nchart.draw(data,options);\n}\nfunction drawChart(){\nfetchTable(mydrawChart);\n}"));
  webpageBody(MEN_INDEX); // </script>\n</head>\n<body>
  webServer.sendContent(F("<div id='title'>\n<h1>Pflanzen Gießmonitor</h1>\n<h3>© 2020-"));
  webServer.sendContent(compiled_year);
  webServer.sendContent(F(" von gebad</h3>\n</div>\n<span class='grid'>\n<table id='dht'>\n<tr>\n<td id='dht1'>\n<input type='button' id='ht250' value='Raum:' onclick='setMeanTbl(\"ht250\")'>\n<data></data>&deg;C\n</td>\n<td id='dht2'>\n<input type='button' id='ht251' value='rel.F:' onclick='setMeanTbl(\"ht251\")'>\n<data></data>&nbsp;%\n</td>\n<td id='wifi'>WiFi&nbsp;&nbsp;<wifi id='rssi'></wifi>&nbsp;dbm</td>\n</tr>\n</table>\n<table id='pft'>\n"));
  for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
    v_type = pgm_read_byte(&calibrateTable[idx].v_type); 
    if (v_type == VT_RAW) {
      message = F("<tr>\n<td id='listpfl'>\n<input type='button'id='pf");
      message.concat(String(idx));
      message.concat(F("'value='"));
      PlateName = getPlateName(idNb - 2);
      if (PlateName.length() > 0) {
        message.concat(PlateName);
      } else {
        legend_len = pgm_read_byte(&calibrateTable[idx].legend_len);
        memcpy_P(str_buffer, (char*)pgm_read_dword(&(calibrateTable[idx].legend)), legend_len);
        legend = String(str_buffer);
        message.concat(legend);
      }
      message.concat(F("'onclick='setMeanTbl(\"pf"));
      message.concat(String(idx));
      message.concat(F("\")'>\n</td>\n<td id='hb'>\n<progress id='humidbar"));
      message.concat(String(idNb));
      message.concat(F("'max='100'value='0'></progress>\n</td>\n<td id='humid'>\n<data></data> %\n</td>\n<td id='desc'></td>\n\n</tr>\n"));
      webServer.sendContent(message);
      idNb++;
    }
  }
  webServer.sendContent(F("</table>\n<BR>\n<div id=\"curve_chart\"></div></span>\n</div>\n</body></html>"));
} // handle_index()

void handle_renewindex() {
  union tcast{  // Typkonvertierung von unsigned Integer (gelesener Hexwert) in signed Integer
    int16_t vi;
    uint16_t vu;
  };
  tcast cast;
  char buffer[6];
  uint8_t v_type;
  uint8_t Sensor = 0;
  uint8_t HumidState;
  uint8_t id = 0;
  String SensorStatus = "";
  
  String message = (F("{"));
  for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
    v_type = pgm_read_byte(&calibrateTable[idx].v_type);
    if (v_type == VT_RAW || v_type == VT_INPGRD || v_type == VT_INPPRZ) {
      message.concat(F("\""));
      message.concat(String(id));
      message.concat(F("\":\""));
      switch (v_type) {
        case VT_INPGRD: {
          cast.vu = DataToDisplay.HT_Sensor.temperature;
          sprintf(buffer, "%0.1f", float(cast.vi) / 10);
          message.concat(buffer);
          break;
        }
        case VT_INPPRZ: {
          message.concat(String((DataToDisplay.HT_Sensor.humidity + 5) / 10));
          break;
        }
        case VT_RAW: {
          HumidState = INAKTIVSENSOR;
          if (Measure.Status[Sensor] != SensorNotDetected) {
            message.concat(String(DataToDisplay.humidPercent[Sensor]));
            if (DataToDisplay.humidPercent[Sensor] >= EEPROM.read(sizeof(calibrate_sensor_t) * Sensor + EEPR_SC0_HUMID)) {
              HumidState = GREEN;
            } else 
            if (DataToDisplay.humidPercent[Sensor] > EEPROM.read(sizeof(calibrate_sensor_t) * Sensor + EEPR_SC0_DRY)) {
              HumidState = YELLOW;
            }
            else {
              HumidState = RED;
            }
          } else {
            message.concat(F("0"));
          }
          message.concat(F("\",\"hstate"));
          message.concat(String(id));
          message.concat(F("\":\""));
          message.concat(String(HumidState));

          switch (Measure.Status[Sensor]) {
            case DataInvalid: {
              SensorStatus = "Daten ungültig. Bitte Sensor kalibrieren (";
              SensorStatus.concat(String(Measure.Percent[Sensor]));
              SensorStatus.concat(F("%)"));
              break;
            }
            case SensorCalibrate:   SensorStatus = "Sensor nicht kalibiert."; break;
            //case SensorNotDetected: SensorStatus = "Sensor nicht angeschlossen"; break;
            default: SensorStatus = F(" "); break;
          }
          message.concat(F("\",\"sstate"));
          message.concat(String(id));
          message.concat(F("\":\""));
          message.concat(String(SensorStatus));
          
          Sensor++;
          break;
        }
      }
      id++;
      message.concat(F("\","));
    }
  }
  message.concat(F("\"tempchart\":\""));
  message.concat(String(showTemperatureChart));
  message.concat(F("\",\"humidchart\":\""));
  message.concat(String(showHumidChart));  
  message.concat(F("\",\"chgtbl\":\""));
  message.concat(String(changeChart));
  message.concat(F("\",\"chtbl\":\""));
  message.concat(String(ChartTable));
  message.concat(F("\",\"rssi\":\""));
  message.concat((String)WiFi.RSSI());
  message.concat(F("\"}"));
  webServer.send(200, "application/json", message);
} // handle_renewindex()

String timeToTblStr(uint16_t ctime) {
  const String dow[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  uint16_t wd, dh;
  
  String message = F(",[\"");
  wd = (ctime >> 10) & 0b111;
  dh = (ctime >> 5) & 0b11111;
  message.concat(String(dh));
  message.concat(F(". "));
  message.concat(dow[wd]); // Wochentag
  message.concat(F(" "));
  message.concat(String(ctime & 0b11111)); // Stunde

  message.concat(F(":00\","));
  return message;
}

void readOldHistSensors(history_sensor_t &ohs, uint32_t adr) {
  int32_t  delAdrVal;

  delAdrVal = adr - (ORDER * HISTSENS_SIZE);
  if (delAdrVal <  RECORD_SIZE) {
    delAdrVal = HISTORYEND - RECORD_SIZE + delAdrVal;
  }
  #ifdef DEBUG
  debug_delAdr = delAdrVal;
  #endif
  binFile.seek(delAdrVal, SeekSet);
  binFile.read(ohs.ar, HISTSENS_SIZE);
}

void handle_renewtable() {
  char buffer[6];
  uint32_t adr;
  record_t record;
  history_sensor_t historySensors, oldHistSensors, prev_histSensors;
  // Variablen für Pflanzen Feuchtigkeitssensoren
  uint16_t sx = 0; // Summe von x über ORDER
  uint8_t  n  = 0;
  uint8_t  nx = 0; // verwendete Ordnung wenn Anzahl lines < ORDER
  uint16_t rx = 0; // result/Ergebnis für Pflanzensensor
  uint8_t  h;
  // Variablen für ht-Sensor
  uint16_t st = 0; // Summe von t(Temperatur) über ORDER
  uint16_t sh = 0; // Summe von h(Luftfeuchtigkeit) über ORDER
  uint8_t zht = 0; // verwendete Ordnung wenn Anzahl lines < ORDER; kann sich von n durch Hysteres unterscheiden
  uint16_t rt = 0; // result/Ergebnis für Temperatur
  uint16_t rh = 0; // result/Ergebnis für Luftfeuchtigkeit

  changeChart = false;
  binFile = LITTLEFS.open(HISTORY_FILENAME, "r");
  binFile.read(record.ar, RECORD_SIZE);
  
  // auch vom Tabellenanfang mit Darstellung beginnen, wenn letzter geschriebener Datensatz == Tebellenende
  if ((record.entries < MAXHISTORYENTRIES) || (record.entries == MAXHISTORYENTRIES && record.writePos == HISTORYEND - HISTSENS_SIZE)) {
    adr = RECORD_SIZE;
  } else {
    // ab nachfolgender Adresse mit Diagramm beginnen
    adr = record.writePos + HISTSENS_SIZE;
  }

  h = EEPROM.read(EEPR_HYSTERESE);

  String message = F("{\"humidchart\":");
  message.concat(String(showHumidChart && !showTemperatureChart));
  message.concat(F(",\"pfls\":\""));
  message.concat(getTitlePlateName(ChartTable));
  message.concat(F("\",\"0\":[[\"Zeit\",\"Feuchtigkeit\""));
  if (showTemperatureChart) {
    message.concat(F(",\"Temperatur\""));
  }
  if (showHumidChart) {
    message.concat(F(",\"rel.Luftfeuchtigkeit\""));
  }
  message.concat(F("]"));
  if (record.entries > 0) {
    for (uint16_t i = 1; i <= record.entries; i++) {
      if (i > 1) {
        prev_histSensors = historySensors; // zur Erkennung Sprung durch Hysterese
      }
      #ifdef DEBUG
      debug_adr = adr;
      #endif
      binFile.seek(adr, SeekSet);
      binFile.read(historySensors.ar, HISTSENS_SIZE);
      message.concat(timeToTblStr(historySensors.DayHour));
      
      if (swShowRollingMean) {
        // https://de.wikipedia.org/wiki/Gleitender_Mittelwert
        
        // gleitender Mittelwert für Pflanzensensoren brechnen
        sx += historySensors.humidPercent[ChartTable];
        if (n <= ORDER ) { // Falls zu wenige Werte für gleitenden Mittelwert vorhanden
          n++;
        }
        #ifdef DEBUG
        debug_delAdr = 0;
        #endif
        oldHistSensors.humidPercent[ChartTable] = 0;
        if (i > 1 && (prev_histSensors.humidPercent[ChartTable] + h <= historySensors.humidPercent[ChartTable])) {
          // keinen gewichteten Mittelwert, wenn Wertesprung >= Hysterese; also wenn gerade gegossen wurde
          n = 1;
          nx = 1;
          sx = historySensors.humidPercent[ChartTable];
        } else {
          // Für gleitenden MW wird kummulativ summiert und erster Wert wird abgezogen, deshalb ORDER+1 Datensätze erforderlich
          if (n > ORDER) { // ab hier muss aller erster Wert wieder abgezogen werden
            readOldHistSensors(oldHistSensors, adr);
            sx -= oldHistSensors.humidPercent[ChartTable];
          } else {
            nx++;
          }
        } 
        rx = sx / nx;
        #ifdef DEBUG
        sprintf(debugBuffer, "i: %d  nx: %d  adr: %X  deladr: %X  Feuchte: %d  Summe F: %d  del F: %d  gMW: %d\r\n", i, nx, debug_adr, debug_delAdr, historySensors.humidPercent[ChartTable], sx, oldHistSensors.humidPercent[ChartTable], rx);
        DebugOut(true, debugBuffer);
        #endif

        // gleitender Mittelwert für HT-Sensor berechnen
        st += historySensors.temperature;
        sh += historySensors.humidity;
        #ifdef DEBUG
        debug_delAdr = 0;
        #endif
        // gleitender Mittelwert Berechnung/Curserpositionierung für HT-Sensor muss auf Grund Hystorie separat erfolgen!
        if (i > ORDER) {
         // War Datensatz oldHistSensors.humidPercent an gleicher Position wie für oldHistSensors.temperature/humid?
          if (n <= ORDER) { // Nein => Cursor neu postionieren und oldHistSensors.ar separat für HT lesen
            readOldHistSensors(oldHistSensors, adr);
          }
          st -= oldHistSensors.temperature;
          sh -= oldHistSensors.humidity;
          zht = ORDER;
        } else {
          oldHistSensors.temperature = 0;
          zht = i;
        }
        rt = st / zht;
        rh = sh / zht;
        #ifdef DEBUG
        sprintf(debugBuffer, "i: %d  zht: %d  adr: %X  deladr: %X  Temperatur: %d  Summe T: %d  del T: %d  gMW: %d\r\n", i, zht, debug_adr, debug_delAdr, historySensors.temperature, st, oldHistSensors.temperature, rt);
        DebugOut(true, debugBuffer);
        #endif
      } else 
      {
        rx = historySensors.humidPercent[ChartTable];
        rt = historySensors.temperature;
        rh = historySensors.humidity;
      }
      
      adr += HISTSENS_SIZE;
      if (adr >= HISTORYEND) {
        adr = RECORD_SIZE;
      }      
      
      sprintf(buffer, "%0.2f", float(rx) / 100);
      message.concat(String(buffer)); // Feuchtigkeit
      if (showTemperatureChart) {
        message.concat(F(","));
        sprintf(buffer, "%0.1f", float(rt) / 10);
        message.concat(buffer);
      }
      if (showHumidChart) {
        message.concat(F(","));
        sprintf(buffer, "%0.2f", float(rh) / 1000);
        message.concat(buffer);
      }
      message.concat(F("]"));
    }
  } else {
    // Falls noch keine Werte in Tabelle
    message.concat(timeToTblStr(currentDayHour));
    if (showTemperatureChart) {
      message.concat(F(",0"));
    }
    if (showHumidChart) {
      message.concat(F(",0"));
    }
    message.concat("0]");
  }

  message.concat(F("]}"));
  webServer.send(200, "application/json", message);
} // handle_renewtable

void handle_calibrate() {
  String message;
  char str_buffer[30]; // z.Z max. Länge String < 30; Buffer Tabellen auslesen von desc
  uint8_t legend_len;
  String legend;
  String last_legend = "";
  String desc;
  uint8_t desc_len;
  uint8_t v_type, swsetX;
  uint8_t val_len;
  uint8_t formnr = 0;
  uint8_t vt_radio = 0;
  String css_class;
  uint16_t maxNumber = 0;
  uint8_t idxNote = 0;
  uint8_t i;
  uint8_t insertNL = false; // steuert zusätzlichen Zeilenvorschub
  uint8_t pf = 0;

  webpageBegin(); // <style>
  webServer.sendContent(F(".inend{float:inline-end;}.grid{display:grid;row-gap:8px;column-gap:10px;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));}\n.grid div{border:0px;margin:1em;margin-bottom:0;}\nselect,option{box-sizing:border-box;font:inherit;padding-bottom:0em;}\ninput{\nfont: inherit;\n}\nlegend {margin:0 auto;font-weight:bold}\n.cmdline {display:flex;align-items:baseline;}\n.cmdblock {float:inline-start;width:13.2em;border:1px,black;}\nlabel {min-width:8em;}\n.cmdparam {width: 5em;margin:0.2em;}\n.misc{\nmargin-left:1.2em;\n}\n.radiobrd{\nborder:1px,black;\n}\n.pfrd{\nwidth:2.3em;\ntext-align:right;\n}\n.pfrp{\nwidth:2.2em;\ntext-align:right;\nline-height:0mm;\n}\n.pfr{\nwidth:3.4em;\ntext-align:right;\nmargin-left:.7em;\n}\n.pfs{\nwidth:2.9em;\ntext-align:right;\nmargin-left:1.2em;\n}\n"));
  webpageScript(); // </style><script>

  /***************************************************************
   *                          script                             *
   ***************************************************************/
  webServer.sendContent(F("function gresize(){\nvar w=295;\nvar gwidth=document.body.offsetWidth-15;\nvar nboxes=Math.ceil(gwidth/w);\nvar lepadding=(gwidth-((nboxes-1)*w))/nboxes;\npaddinl='0 0 0 '+lepadding+'px';\ndocument.querySelector('.grid').style.padding=paddinl;\n}\ndocument.addEventListener('DOMContentLoaded',()=>{\noncecalib(),gresize(),renewcalib();\nwindow.addEventListener('resize',gresize);\nwindow.setInterval(renewcalib,"));
  webServer.sendContent(String(POLL_INTERVALL));
  webServer.sendContent(F(");\n});\nfunction renewcalib(){\nfetch('/renewcalib').then((resp) =>{\nreturn resp.json();\n}).then((obj)=>{\nlet data=document.querySelectorAll('data');\nfor(i=0;i<"));
  message = String(MaxSensors);
  message.concat(F("*4+2;i++){\ndata[i].innerHTML=obj[i];\n}\nif(obj['swLED']==0){\nlet colors=document.querySelectorAll('[name=colors]');\ncolors[obj['colors']].checked=true;\nvar rgb=obj['id"));
  i = 0;
  while (i < CALIBRATE_TABLE_LINES && pgm_read_byte(&calibrateTable[i].v_type) != VT_INP_LED_RD) {
    i++;
  }
  message.concat(String(i));
  message.concat(F("'];\ndocument.querySelector('#id"));
  message.concat(String(i));
  message.concat(F("').value=rgb[0];\ndocument.querySelector('#id"));
  message.concat(String(i+1));
  message.concat(F("').value=rgb[1];\ndocument.querySelector('#id"));
  message.concat(String(i+2));
  message.concat(F("').value=rgb[2];\n}\n});\n}\nfunction oncecalib(){\nlet colors=document.querySelectorAll('[name=colors]');\nfetch('/oncecalib').then((resp)=>{\nreturn resp.json();\n}).then((obj)=>{\n"));
  for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
    v_type = pgm_read_byte(&calibrateTable[idx].v_type);
    if (!(v_type & MASK_SHOW) & (v_type != VT_MINMAXDEL)) { // nur für once
      val_len = pgm_read_byte(&calibrateTable[idx].val_len);
      if (v_type == VT_RADIO) {
        if (val_len == 3) {
          message.concat(F("colors[obj['"));
          message.concat(String(idx));
          message.concat(F("']].checked = true;\n"));
        }
      }
      else {
        message.concat(F("document.querySelector('#id"));
        message.concat(String(idx));
        message.concat(F("')."));
        switch(v_type) {
          case VT_ONOFF: {
            if (val_len == SW_SHOWLED) {
              idxNote = idx;
            }
          }
          default: {
            message.concat(F("value=obj['"));
            message.concat(String(idx));
            message.concat(F("'];\n"));
          }
        }
      }
    }
  }
  message.concat(F("activateLEDcolor(obj['"));
  message.concat(String(idxNote));
  message.concat(F("']);"));
  webServer.sendContent(message);
  webServer.sendContent(F("});\n}\nfunction activateLEDcolor(ena){\nvar state=(ena==0);\ndocument.querySelector('#fieldLED').disabled=state;\n}\nfunction setclick(){\nlet calibrateData=new FormData();\nvar temp=arguments[0].substr(2)+\",\"+'0'+\"\";\ncalibrateData.append('setVal',temp);\nsend(calibrateData);\nwindow.setTimeout(\"renewcalib()\",10);\n}\nfunction setcfg(){\nif(arguments[0]==\"id"));
  webServer.sendContent(String(idxNote));
  webServer.sendContent(F("\"){\nactivateLEDcolor(document.getElementById(arguments[0]).value);\n}\nlet calibrateData=new FormData();\nvar temp=arguments[0].substr(2)+\",\"+arguments.length+\"\";\nfor(i=0;i<arguments.length;i++){\nvar element=document.getElementById(arguments[i]);\nif(element!=null){\nvalue=element.value;\ntemp+=\",\"+String(value)+\"\";\n}\n}\ncalibrateData.append('setVal',temp);\nif(element.type=='radio'){\nwindow.setTimeout(\"oncecalib()\",10);\n}\nsend(calibrateData);\n}\nfunction send(arg){\nfetch('/setrequest',{\nmethod:'post',\nbody:arg\n});\n}"));
  webpageBody(MEN_CALIB); // </script>\n</head>\n<body>
  /***************************************************************
   *                           body                              *
   ***************************************************************/
  webServer.sendContent(F("<div id='title'></div><div class='grid'>\n"));
  for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
    message = "";
    v_type = pgm_read_byte(&calibrateTable[idx].v_type);
    if (vt_radio == 1 && v_type != VT_RADIO) {
      vt_radio++;
      message.concat(F("</fieldset>"));
    }

    legend_len = pgm_read_byte(&calibrateTable[idx].legend_len);
    if (legend_len > 1) {
      memcpy_P(str_buffer, (char*)pgm_read_dword(&(calibrateTable[idx].legend)), legend_len);
      legend = String(str_buffer);
      if (last_legend == "") last_legend = legend;
      if (last_legend != legend) {
        last_legend = legend;
        message.concat(F("</fieldset>")); // Legenden-Block
      }
      message.concat(F("\n<fieldset class='cmdblock'"));
      if (v_type == VT_RADIO) {
        message.concat(F(" id='fieldLED'"));
      }
      message.concat(F(">\n<legend>")); // Legenden-Block
      if (v_type == VT_RAW) {
        message.concat(getTitlePlateName(pf));
        pf++;
      } else {
        message.concat(legend);
      }
      message.concat(F("</legend>\n"));
    }
    
    if (v_type == VT_RADIO && vt_radio == 0) {
      vt_radio++;
      message.concat(F("<fieldset class='radiobrd'>\n"));
    }
    // zusätzliche Leerzeile zwischen Radio, RAW-Anzeigen und Werte Eingaben einfügen
    if (v_type == VT_MINMAXDEL || (insertNL && !(v_type & MASK_SHOW) && v_type != VT_RADIO)) {
      message.concat(F("<br>\n"));
      insertNL = false;
    }
    message.concat(F("<block class='cmdline'>\n<label>"));
    if (v_type == VT_RADIO) {
      message.concat(F("&nbsp;&nbsp;")); // Leerzeichen vor Auswahl LED-Anzeige
      insertNL = true;
    }
    desc_len = pgm_read_byte(&calibrateTable[idx].desc_len);
    memcpy_P(str_buffer, (char*)pgm_read_dword(&(calibrateTable[idx].desc)), desc_len);
    desc = String(str_buffer);
    if (v_type == VT_MINMAXDEL) {
      message.concat(F("<input type='button'id='id"));
      message.concat(String(idx));
      message.concat(F("'value='"));
      message.concat(desc);
      message.concat(F("'onclick='setclick(\"id")); // sonst gibt es Probleme mit value="string"
      message.concat(String(idx));
      message.concat(F("\")'>"));
    } else {
      message.concat(desc);
    }
    message.concat(F("</label>\n<block class='cmdparam'>"));
    formnr++;

    val_len = pgm_read_byte(&calibrateTable[idx].val_len);
    int value = 0;
    switch (v_type) {
      case VT_MINMAXDEL: break;
      case VT_ONOFF: {
        message.concat(F("\n<select id='id"));
        message.concat(String(idx));
        message.concat(F("'class='misc'onchange='setcfg(\"id"));
        message.concat(String(idx));
        if (val_len != SW_SHOWLED) {  // zweiter Wert nur benötigt Gleitender MW VT_ONOFF soll gleich 
          message.concat(F("\",\"id")); // ohne speichern reagieren, aber bei Set speichern
          message.concat(String(idx));// Kann dadurch mit Anzahl Argumente (2 oder 1) unterschieden werden
        }
        message.concat(F("\")'>\n<option value='0'>Aus</option>\n<option value='1'>Ein</option>\n</select>\n"));
        break;
      }
      case VT_RADIO: {
        message.concat(F("<input type=radio name='colors'id='id"));
        message.concat(String(idx));
        message.concat(F("'value='"));
        message.concat(String(pgm_read_byte(&calibrateTable[idx].val_len)));
        message.concat(F("'onchange='setcfg(\"id"));
        message.concat(String(idx));
        message.concat(F("\")'>\n"));
        break;
      }
      case VT_INPGRD:
      case VT_INPPRZ:
      case VT_INP_LED_RD:
      case VT_INP_LED_GN:
      case VT_INP_LED_BL:
      case VT_INP_RAW:
      case VT_HYSTERESE:
      case VT_INP_TRI: {
        if (v_type == VT_INP_RAW || v_type == VT_INPGRD || v_type == VT_INPPRZ) {
          css_class = "pfr";
        } else {
          css_class = "pfs";
        }
        message.concat(F("\n<input type=number class='"));
        message.concat(css_class);
        message.concat(F("'id='id"));
        message.concat(String(idx));
        message.concat(F("'value='"));
        message.concat(String(value));
        if (v_type == VT_INPGRD || v_type == VT_INPPRZ) {
            message.concat(F("'step='0.1'min='-29.9'max='29.9"));
        } else {
          switch(v_type) {
            case VT_INP_LED_RD:
            case VT_INP_LED_GN:
            case VT_INP_LED_BL: maxNumber = 255; break;
            case VT_INP_RAW:    maxNumber = 3400; break;
            case VT_HYSTERESE:
            case VT_INP_TRI:    maxNumber = 99; break;
          }
          message.concat(F("'min='0'max='"));
          message.concat(String(maxNumber));
        }
        switch(v_type) {
          case VT_INP_LED_RD:
          case VT_INP_LED_GN:
          case VT_INP_LED_BL: {
            message.concat(F("'onchange='setcfg(\"id"));
            message.concat(String(idx));      
            message.concat(F("\")"));
          }
        }
        message.concat(F("'>"));
        break;
      }
      default: { // keine Eingabe möglich, da nur zur Anzeige
        message.concat(F("\n<div class='pfrp'id='value"));
        message.concat(String(formnr));
        message.concat(F("'value='"));
        message.concat(String(value));
        message.concat(F("'><data></data></div>"));
        insertNL = true;
        break;
      }
    }

    swsetX = pgm_read_byte(&calibrateTable[idx].swsetX);
    if (swsetX > 0) {
      message.concat(F("\n</block>\n<input type=button value='Set'onclick='setcfg(\"id"));
      for (uint8_t i = 0; i < swsetX; i++) {
        message.concat(String(idx + i));
        if ((i +1 ) < swsetX) {
          message.concat(F("\",\"id"));
        }
      }
      message.concat(F("\")'>"));
    } else message.concat(F("</block>\n"));

    message.concat(F("</block>\n")); // da sonst </block> vorher </a></block><input
    webServer.sendContent(message);
  }
  webServer.sendContent(F("</fieldset>\n<div class=inend></div></div></div>\n</body></html>"));
} // handle_calibrate()

void handle_setrequest() {
  uint8_t i = 0;
  int intArgs[5]; // Byte 0 -> line, bzw. idx; Byte 1 -> Anzahl folgender Werte; Byte 2 bis -> Werte
  float floatArg = 0; // füe Offset Temperatur oder Luftfeuchte
  char str[20];
  char* oneArg;
  uint8_t idx;
  uint8_t v_type;
  uint16_t adrEEPR;
  uint8_t val_len; // Anzahl value Bytes oder bei NO_EEPROM value für radio oder im RAM speichern bei Wert 3
  uint8_t chart_table;

  if (!webServer.authenticate(http_user, http_pass)) {
    return webServer.requestAuthentication();
  }

  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "0");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");
  webServer.sendContent(F("<!DOCTYPE html><HTML lang='de'><head><meta charset='utf-8'><style>\nhtml{font-family: Arial;}iframe{font: inherit;}</style>"));
  // style font wichtig für Darstellung Text

  strcpy (str, webServer.arg("setVal").c_str());
  #ifdef DEBUG
  sprintf(debugBuffer, "Argumentenstring: %s\r\n", webServer.arg("setVal").c_str());
  DebugOut(true, debugBuffer);
  #endif
  oneArg = strtok(str, ",");
  while (oneArg != NULL) {
    intArgs[i] = atoi(oneArg);
    if (i == 2) floatArg = atof(oneArg);
    oneArg = strtok(NULL, ",");
    i++;
  }
  
  #ifdef DEBUG
  for (int i = 0; i < intArgs[1]+2; i++) {
    sprintf(debugBuffer,"Arg %d ist %d\r\n", i, intArgs[i]);
    DebugOut(true, debugBuffer);
  }
  sprintf(debugBuffer,"Arg Float ist %f\r\n", floatArg);
  DebugOut(true, debugBuffer);
  #endif


  idx = intArgs[0];// Gleitender MW VT_ONOFF soll gleich ohne speichern reagieren, aber bei Set speichern
  v_type = pgm_read_byte(&calibrateTable[idx].v_type);
  adrEEPR = pgm_read_dword(&calibrateTable[idx].adrEEPR) & 0xffff;
  val_len = pgm_read_byte(&calibrateTable[idx].val_len);
  
  // idx 250 oder 252 sind repetierende Schalter für Grafik Temperatur/rel.F. und sind weder
  // in calibrateTable noch in configTable enthalten
  if (idx == 250) {
    showTemperatureChart = !showTemperatureChart;
    changeChart = true;
  } else
  if (idx == 251) {
    showHumidChart = !showHumidChart;
    changeChart = true;
  } else
  if (adrEEPR > 0) {
    if (v_type == VT_INPGRD || v_type == VT_INPPRZ) {
      EEPROM.writeShort(adrEEPR, int16_t(floatArg * 10));
      EEPROM.commit();
    } else
    if (v_type == VT_ONOFF && (intArgs[1] == 2)) { // Dieser ONOFF-Wert gMW wird ausnahmsweise mit Set gespeichert und bedarf deshalb keine ONOFF-Unterscheidung
      swShowRollingMean = intArgs[2]; // Schalter gleitender Mittelwert
    } else {
      // derzeit laut Tabelle gibt es für alle anderen im EEPROM zu speichernde Werte jeweils nur ein zu speicherndes Element
      for (uint8_t i = 0; i < val_len; i++) { // array[val_len] => int...long
        EEPROM.write(adrEEPR + i, (intArgs[2] >> (8 * i)) & 0xFF);
      }
      EEPROM.commit();
    }
  } else {
    switch (v_type) {
      case VT_RADIO: {
        ledColors = LedState(intArgs[2]);
        EEPROM.get(ledColors * sizeof(rgbt_t) + EEPR_LED_DRY, led_rgb);
        break;
      }
      case VT_INP_LED_RD: {
        led_rgb.rd = intArgs[2];
        if (intArgs[1] == 3) { // mit set betätigt und somit im EEPROM speichern
          led_rgb.gn = intArgs[3];
          led_rgb.bl = intArgs[4];
          EEPROM.put(ledColors * sizeof(rgbt_t) + EEPR_LED_DRY, led_rgb);
          EEPROM.commit();
        }
        break;
      }
      case VT_INP_LED_GN: {
        led_rgb.gn = intArgs[2];
        break;
      }
      case VT_INP_LED_BL: {
        led_rgb.bl = intArgs[2];
        break;
      }
      case VT_ONOFF: {
        swSetLEDcolor = intArgs[2]; // zu VT_ONOFF SW_GMW wird derzeit durch nicht/vorhanden idxEEPROM unterschieden
        if (swSetLEDcolor) {
          EEPROM.get(ledColors * sizeof(rgbt_t) + EEPR_LED_DRY, led_rgb);
        }
        break;
      }
      case VT_MINMAXDEL: {
        for (uint8_t i = 0; i < MaxSensors; i++) { // Mit Betätigung Min- und Maxwerte zurücksetzen
          Measure.RAW_Max[i] = 0;
          Measure.RAW_Min[i] = 0xFFFF;
        }
        break;
      }
      case VT_RAW: { // kommt aus handle_index für Grafik Pflanzensensor
        // idx entspricht Tabellenzeile
        chart_table = 0;
        for (uint8_t pft = 0; pft < CALIBRATE_TABLE_LINES; pft++) {
          if (idx == pft) { // gezählte VT_RAW mit NO_EEPROM ist gleich ChartTable 0 bis 5
            #ifdef DEBUG
            sprintf(debugBuffer, "Pflanzens. %d\r\n", chart_table);
            DebugOut(debugBuffer);
            #endif
            ChartTable = chart_table;
            changeChart = true;
            break;
          }
          uint8_t v = pgm_read_byte(&calibrateTable[pft].v_type);
          // VT_RAW mit NO_EEPROM zählen bis Tabellenzeile idx erreicht wurde
          if (v == VT_RAW) {
            chart_table++;
          }
        }
        break;
      }
    }
    if (v_type == VT_RADIO || val_len >= 3) { // es gibt nure einen val_len >= 3 Eintrag, sonst könnte damit nicht unterschieden werden
      SetLed();
    }
  }
} // handle_setrequest()

void handle_oncecalib() {
  union tcast{ // Typkonvertierung von unsigned Integer (gelesener Hexwert) in signed
    int16_t vi;
    uint16_t vu;
  };
  tcast cast;
  uint8_t v_type;
  uint16_t adrEEPR;
  uint8_t val_len; // Anzahl value Bytes oder bei NO_EEPROM value für radio oder im RAM speichern bei Wert 3
  long value;
  char buffer[10];

  String message = (F("{"));
  for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
    v_type = pgm_read_byte(&calibrateTable[idx].v_type);
    adrEEPR = pgm_read_dword(&calibrateTable[idx].adrEEPR) & 0xffff;
    val_len = pgm_read_byte(&calibrateTable[idx].val_len);
    if (adrEEPR > 0 || val_len >= 3) { // 3 von der Aufzählung für html-Boxen nur ein Wert speichern und VT_ONOFF unterscheiden
      message.concat(F("\""));
      message.concat(String(idx));
      message.concat(F("\":\""));
      if (adrEEPR > 0) { // sonst wird Variable im RAM gespeichert
        value = 0;
        for (uint8_t i = 0; i < val_len; i++) {
          value |= (EEPROM.read(adrEEPR + val_len - 1 - i) << (8 * (val_len -1 - i)));
        }
        switch (v_type) {
          case VT_INPGRD:
          case VT_INPPRZ: {
            cast.vu = value;
            sprintf(buffer, "%0.1f", float(cast.vi) / 10);
            message.concat(buffer); 
            break;
          }
          case VT_ONOFF: { // zu VT_ONOFF SW_SHOWLED wird derzeit durch nicht/vorhanden idxEEPROM unterschieden
            message.concat(String(swShowRollingMean));
            break;
          }
          default: message.concat(String(value)); break;
        }
      } else 
      { // für radio-Box, LED oder Schalter => RAM-Werte
        if (val_len >= 3) { // 3 von der Aufzählung für html-Boxen nur ein Wert speichern und VT_ONOFF unterscheiden
          switch (v_type) {
            case VT_RADIO: {
              message.concat(String(ledColors));
              break;
            }
            case VT_INP_LED_RD: {
              EEPROM.get(ledColors * sizeof(rgbt_t) + EEPR_LED_DRY, led_rgb);
              message.concat(String(led_rgb.rd));
              break;
            }
            case VT_INP_LED_GN: {
              message.concat(String(led_rgb.gn));
              break;
            }
            case VT_INP_LED_BL: {
              message.concat(String(led_rgb.bl));
              break;
            }
            case VT_ONOFF: { // zu VT_ONOFF SW_GMW wird derzeit durch nicht/vorhanden idxEEPROM unterschieden
              message.concat(String(swSetLEDcolor));
              break;
            }
          }
        }
      }
      message.concat(F("\","));
    }
  }
  message.remove(message.length()-1);  // (",") löschen
  message.concat(F("}"));
  webServer.send(200, "application/json", message);
} // handle_oncecalib

void handle_renewcalib() {
  uint8_t v_type;
  uint8_t Sensor = 0;
  uint8_t id = 0;
  char buffer[6];

  String message = (F("{"));
  for (uint8_t idx = 0; idx < CALIBRATE_TABLE_LINES; idx++) {
    v_type = pgm_read_byte(&calibrateTable[idx].v_type);
    if (v_type & MASK_SHOW) {
      message.concat(F("\""));
      message.concat(String(id));
      id++;
      message.concat(F("\":\""));
      switch (v_type) {
        case VT_ROOMT: {
          sprintf(buffer, "%0.1f", HTMeasure.Temperature);
          message.concat(buffer);
          message.concat(F("&deg;C"));
          break;
        }
        case VT_ROOMH: {
          sprintf(buffer, "%0.1f", HTMeasure.Humidity);
          message.concat(buffer);
          message.concat(F("&nbsp;%"));
          break;
        }
        case VT_RAW: {
          message.concat(String(Measure.Humid_RAW[Sensor]));
          break;
        }
        case VT_RAW_MIN: {
          message.concat(String(Measure.RAW_Min[Sensor]));
          break;
        }
        case VT_RAW_MAX: {
          message.concat(String(Measure.RAW_Max[Sensor]));
          break;
        }
        case VT_PROZENT: {
          message.concat(String(Measure.RAW_Pct[Sensor]));
          message.concat(F("&nbsp;%"));
          Sensor++;
          break;
        }
      }
      message.concat(F("\","));
    }
    switch(v_type) {
      case VT_INP_LED_RD: {
        message.concat(F("\"id"));
        message.concat(String(idx));
        message.concat(F("\":["));
        message.concat(String(led_rgb.rd));
        message.concat(F(","));
        message.concat(String(led_rgb.gn));
        message.concat(F(","));
        message.concat(String(led_rgb.bl));
        message.concat(F("],"));
        break;
      }
    }
  }
  message.concat(F("\"swLED\":\""));
  message.concat(String(swSetLEDcolor));
  message.concat(F("\",\"colors\":\""));
  message.concat(String(ledColors));
  message.concat(F("\"}"));
  webServer.send(200, "application/json", message);
} // handle_renewcalib()

void handle_config() {
  // Bei der Konfiguration werden die Eingabefelder nicht mittels fetch aktulisiert, sondern
  // mit Erstellung der Webseite bereits ausgefüllt.
  // Nur für die Anzeige "Freier dynamischer Speicher" wird fetch verwendet.
  // Auch die Übertragung der Eingabefelder zur Auswertung erfolgt nicht mit fetch, sondern
  // über window.open(param_string, 'output')
  String message;
  char str_buffer[30]; // z.Z max. Länge String < 30; Buffer Tabellen auslesen von desc
  uint8_t legend_len;
  String legend;
  String last_legend = "";
  String desc;
  uint8_t desc_len;
  uint8_t v_type, swsetX;
  unsigned int idxEEPROM;
  uint8_t maxlen;
  String strval;
  IPAddress manual_ipnet;

  webpageBegin(); // <style>
  webServer.sendContent(F(".inend{float:inline-end;}.grid{display:grid;row-gap:8px;column-gap:2px;grid-template-columns:repeat(auto-fit,minmax(420px,1fr));}\n.grid div{border:0px;margin:1em;margin-bottom:0;}\nselect,option{box-sizing:border-box;font:inherit;padding-bottom:0em;}\n.inp{\ntext-align:left;\nwidth:11.5em;\n}\n.inpoz{\ntext-align:left;\nwidth:3.7em;\n}\n.inpnet{\ntext-align:left;\nwidth:7.8em;\nposition:relative;\nleft:17.3%;\ntransform:(0,0);\n}\n.tlc{\ntext-align:center;\n}\ninput{\nfont: inherit;\n}\nlegend {margin:0 auto;font-weight:bold}\n.cmdline {display:flex;align-items:baseline;}\n.cmdblock {float:inline-start;width:23.8em;border:1px,black;}\nlabel {min-width:8em;}\n.cmdparam {width:12em;margin:0.2em;}\n.cons p{margin:4px 0 0 0;}"));
  webpageScript(); // </style><script>

  /***************************************************************
   *                          script                             *
   ***************************************************************/
  webServer.sendContent(F("function setcfg(){\nparam_string=\"setcfg?\";\nfor (i=0;i<arguments.length;i++) {\nvar element=document.getElementById(arguments[i]);\nif (element!=null){\nif(element.type=='checkbox'){\nvalue=element.checked==true;\n} else {\nvalue=element.value;\n}\nparam_string=param_string+arguments[i]+\"=\"+encodeURIComponent(value);\nif(arguments.length>0&&i<arguments.length-1){\nparam_string=param_string+\"&\";\n}\n}\n}\nwindow.open(param_string,'output');}\nfunction gresize(){\nvar gwidth=document.body.offsetWidth-15;\nvar nboxes=Math.ceil(gwidth/428);\nvar lepadding=(gwidth-((nboxes-1)*428))/nboxes;\npaddinl='0 0 0 '+lepadding+'px';\ndocument.querySelector('.grid').style.padding=paddinl;\n}\ndocument.addEventListener('DOMContentLoaded',()=>{\nfheap(),gresize();\nwindow.addEventListener('resize',gresize);\nlet frh=document.querySelector('#frh');\nfunction fheap(){\nfetch('/fheap').then((resp)=>{\nreturn resp.json();\n}).then((obj)=>{\nfrh.innerHTML=obj['fheap'];\n});\n};\nwindow.setInterval(fheap,2000);\n});"));
  webpageBody(MEN_CONFIG); // </script>\n</head>\n<body>
  /***************************************************************
   *                           body                              *
   ***************************************************************/
  webServer.sendContent(F("<div id='title'></div><div class='grid'>\n"));

  for (uint8_t idx = 0; idx < CONFIG_TABLE_LINES; idx++) {
    message = "";
    legend_len = pgm_read_byte(&configTable[idx].legend_len);
    if (legend_len > 1) {
      memcpy_P(str_buffer, (char*)pgm_read_dword(&(configTable[idx].legend)), legend_len);
      legend = String(str_buffer);
      if (last_legend == "") last_legend = legend;
      if (last_legend != legend) {
        last_legend = legend;
        message.concat(F("</fieldset>")); // Legenden-Block
      }
      message.concat(F("\n<fieldset class='cmdblock'>\n<legend>")); // Legenden-Block
      message.concat(legend);
      message.concat(F("</legend>\n"));
    }
    message.concat(F("<block class='cmdline'>\n<label>"));
    
    desc_len = pgm_read_byte(&configTable[idx].desc_len);
    memcpy_P(str_buffer, (char*)pgm_read_dword(&(configTable[idx].desc)), desc_len);
    desc = String(str_buffer);
    message.concat(desc);
    message.concat(F("</label>\n<block class='cmdparam'>"));


    idxEEPROM = pgm_read_dword(&configTable[idx].idxEEPROM) & 0xffff;
    v_type = pgm_read_byte(&configTable[idx].v_type);
    
    switch (v_type) {
      case VT_CHECK: {
        message.concat(F("<input class=inp type=checkbox id=id"));
        message.concat(String(idx));
        if (EEPROM.read(idxEEPROM) == 1)
          message.concat(F(" CHECKED"));
        message.concat(F(">"));
        break;
      }
      case VT_INTLIST: {
        message.concat(F("\n<select class='inpoz'id='id"));
        message.concat(String(idx));
        message.concat(F("'>\n"));
        for (int8_t x = -12; x < 13; x++) {
          message.concat(F("<option value='"));
          message.concat(String(x));
          if ((int8_t)EEPROM.read(idxEEPROM) == x) {
            message.concat(F("' selected>"));
          } else
            message.concat(F("'>"));
          if (x > 0 ) {
            message.concat(F("+"));
          }
          message.concat(String(x));
          message.concat(F("</option>\n"));
        }
        message.concat(F("</select>\n"));  
        break;
      }
      default: {
        message.concat(F("<input class='"));
        if (v_type == VT_IPNET) {
          message.concat(F("inpnet"));
        } else {
          message.concat(F("inp"));
        }
        message.concat(F("'type=text "));
        maxlen = pgm_read_byte(&configTable[idx].maxlen);
        if (maxlen > 0) {
          message.concat(F("maxlength='"));
          message.concat(String(maxlen));
          message.concat(F("'"));
        }
        message.concat(F("id=id"));
        message.concat(String(idx));
        message.concat(F(" value='"));

        switch (v_type) {
          case VT_IPNET: {
            manual_ipnet = EEPROM.readLong(idxEEPROM);
            strval = manual_ipnet.toString();
            break;
          }
          case VT_STRING: {
            strval = EEPROM.readString(idxEEPROM);
            break;
          }
        }
        if (strval.length() > 0) {
          message.concat(strval);
        }
        message.concat(F("'>\n"));
        break;
      }
    }
    message.concat(F("</block>\n"));
    swsetX = pgm_read_byte(&configTable[idx].swsetX);
    if (swsetX > 0) {
      message.concat(F("<input type=button onclick='setcfg(\"id"));
      for (uint8_t i = 0; i < swsetX; i++) {
        message.concat(String(idx + i));
        if ((i + 1) < swsetX) message.concat(F("\",\"id"));
      }
      message.concat(F("\")' value='Setzen'>\n"));
    }
    message.concat(F("\n</block>\n"));

    webServer.sendContent(message);
  }
  webServer.sendContent(F("</fieldset>\n<div class=inend></div></div>"));
  webpageEnd(MEN_CONFIG); // </div>\n</div>\n<block class=cons>....</div>\n</body></html>
} // handle_config

// Auswertung Kommandoeingabe auf Webseite für Konfiguration z.B. SSID, IP usw.
void handle_setcfg() {
  String message;
  uint8_t v_type, category, swsetX;
  unsigned int idxEEPROM;
  uint8_t idx = 0;
  int intval;
  String strval;
  String argn;
  String tmparg;
  IPAddress manual_ipnet;
  boolean checkChanged;
  char ntpServer[EEPROM_OFFSETTZ-EEPROM_NTPSERVER] = { 0 };
  
  if (!webServer.authenticate(http_user, http_pass)) {
    return webServer.requestAuthentication();
  }
  
  tmparg = webServer.argName(0); // argName ist id + Tabellenzeile mit 0 beginnend
  tmparg = tmparg.substring(2, tmparg.length());
  idx = atoi(tmparg.c_str());
  swsetX = pgm_read_byte(&configTable[idx].swsetX);
  idxEEPROM = pgm_read_dword(&configTable[idx].idxEEPROM) & 0xffff;
  v_type = pgm_read_byte(&configTable[idx].v_type);
  category = pgm_read_dword(&configTable[idx].category);
  message = "";
  
  #ifdef DEBUG
  sprintf(debugBuffer, "v_type: %x idx: %d  swsetX: %d\r\n", v_type, idx, swsetX);
  DebugOut(true, debugBuffer);
  #endif
  switch (v_type) {
    case VT_CHECK: {
      intval = webServer.arg(0).length() < 5; // Länge 5 hat Wert "false"
      checkChanged = EEPROM.read(idxEEPROM);
      checkChanged = (checkChanged != intval);
      if (checkChanged) EEPROM.write(idxEEPROM, intval); // Meldungen sollen trotzem erfolgen
      message = ((boolean)intval ? F("Aktiviere ") : F("Deaktiviere "));
      
      // da VT_CHECK in Tabelle nur einmal vorkommt ist keine Kategorieabfrage erforderlich
      message.concat(F("manuelle Netzwerkeinstellungen.")); // Neustart/reload ipnet erforderlich initializeWiFi
      // x = 1, da mit VT_CHECK oben bereits EEPROM.write für De-/Aktivierung
      for (uint8_t x = 1; x < swsetX; x++) {
        idxEEPROM = pgm_read_dword(&configTable[idx + x].idxEEPROM);
        argn = "id" + String(idx + x); // alle Argumente (z.B.: id12 und id13) holen und speichern
        manual_ipnet.fromString("0.0.0.0");
        manual_ipnet.fromString(webServer.arg(argn));
        //writeEEPROMlong(idxEEPROM, manual_ipnet);
        EEPROM.writeLong(idxEEPROM, manual_ipnet);
      }
      if (checkChanged) {
        message.concat(F("<br>Programm startet in einer Minute neu!"));
        wifi_failed = true;
        starttime = millis() + 60000;
      }
      break;
    }
    case VT_INTLIST : {
      tzOffset = atoi(webServer.arg(0).c_str());
      EEPROM.write(EEPROM_OFFSETTZ, tzOffset);
      message = F("Setze Offset Zeitzone");
      EEPROM.get(EEPROM_NTPSERVER, ntpServer);
      configTime(tzOffset * 3600, daylightOffset_sec, ntpServer);
      break;
    }
    case VT_STRING: {
      for (uint8_t x = 0; x < swsetX; x++) { // z.B.: beim Login werden gleichzeitig 2 Argumente gespeichert
        idxEEPROM = pgm_read_dword(&configTable[idx + x].idxEEPROM) & 0xffff;
        argn = "id" + String(idx + x); // alle Argumente (z.B.: id12 und id13) holen und speichern
        strval = webServer.arg(argn);
        EEPROM.writeString(idxEEPROM, strval);
      }
      message = F("Setze ");
      switch (category) { // es wird der numerische Teil von argName(0) ausgewertet
        case  CAT_WLAN: {
            message.concat(F("Wifi Anmeldedaten.<br>Programm startet in einer Minute neu!")); 
            wifi_failed = true;
            starttime = millis() + 60000;
          }
          break;
        case  CAT_HTTP: {
            message.concat(F("HTTP Anmeldedaten.")); 
            EEPROM.get(EEPROM_HTTPUSER, http_user);
            EEPROM.get(EEPROM_HTTPPASS, http_pass);
          }
          break;
        case  CAT_NTP: {
            message.concat(F("NTP-Server."));
            EEPROM.get(EEPROM_NTPSERVER, ntpServer);
            configTime(tzOffset * 3600, daylightOffset_sec, ntpServer);
          }
          break;
        case CAT_PNAME: {
            message.concat(F("Pflanzenname."));
          }
          break;
      }
      break;
    }
  }
  EEPROM.commit();
  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "0");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");
  webServer.sendContent(F("<!DOCTYPE html><HTML lang='de'><head><meta charset='utf-8'><style>\nhtml{font-family: Arial;}iframe{font: inherit;}</style>"));
  webServer.sendContent(message);
} //handle_setcfg

String displayDump (int start, int end) {
  String message = "";
  for (int i = start; i < end; i++) {
    if ((i - start) % 16 == 0) {  //  i - start Adressspalten sonst falsch zugeordnet
      message.concat(F("\n"));
      message.concat(to_hex((i >> 8) & 0xff));
      message.concat(to_hex(i));
      message.concat(F(":"));
    }
    message.concat(F(" "));
    message.concat(to_hex(EEPROM.read(i)));
  }
  return message;
}

String displayFile(String fileName, String sprefix) {
  record_t record;
  char buffer[6];
  String message = "";
  uint32_t calcSize;
  
  if (LITTLEFS.exists(fileName)) {
    binFile = LITTLEFS.open(fileName, "r");
    binFile.read(record.ar, RECORD_SIZE);
    message.concat(F("\n\n"));
    message.concat(sprefix);
    message.concat(F("Historie Sensoren\n"));
    message.concat(sprefix);
    message.concat(F("HISTORY_ADR: "));
    sprintf(buffer, "%X\n", record.writePos);
    message.concat(String(buffer));
    message.concat(sprefix);
    message.concat(F("HISTORY_END-ADR: "));
    sprintf(buffer, "%X\n", HISTORYEND);
    message.concat(String(buffer));
    message.concat(sprefix);
    message.concat(F("HISTORY_ENTRIES: "));
    message.concat(String(record.entries));
    calcSize = binFile.size() - RECORD_SIZE;
    for (uint32_t i = 0; i < calcSize; i++) {
      if (i % sizeof(history_sensor_t) == 0) {
        message.concat(F("\n"));
        sprintf(buffer, "%4X", i + RECORD_SIZE);
        message.concat(String(buffer));
        message.concat(F(":"));
      }
      message.concat(F(" "));
      message.concat(to_hex(binFile.read()));
    }
    binFile.close();
  }
  return message;
}

// Inhalt Speicher ValMem, Protokolltabellen, EEPROM
void handle_dump() {
  String message = F("<PRE>");

  if (!webServer.authenticate(http_user, http_pass)) {
    return webServer.requestAuthentication();
  }

  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "0");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");

  message.concat(F("\n\nEEPROM\n"));
  message.concat(displayDump(0, EEPROMDATEND));
  message.concat(F("\n\nKalibierungswerte\n"));
  message.concat(displayDump(EEPR_HTS_TEMP, EEPR_END));
  message.concat(displayFile(HISTORY_FILENAME, ""));
  webServer.sendContent(message);
  message = displayFile(BACKUP_FILENAME, "Backup-");
  webServer.sendContent(message);
  message = displayFile(TEMP_FILENAME, "TEMP-");
  webServer.sendContent(message);
} //handle_dump

void startWebServers() {
  webServer.begin();
  webServer.on("/", handle_index);
  webServer.on("/renewindex", handle_renewindex);
  webServer.on("/renewtable", handle_renewtable);
  webServer.on("/calibrate", handle_calibrate);
  webServer.on("/setrequest", handle_setrequest);
  webServer.on("/oncecalib", handle_oncecalib);
  webServer.on("/renewcalib", handle_renewcalib);
  webServer.on("/config", handle_config);
  webServer.on("/setcfg", handle_setcfg);
  webServer.on("/fheap", handle_freeheap);
  webServer.on("/dump", handle_dump);
  DebugOut(true, F("HTTP Server gestartet"));
}

void StartUpdateServer() {
  updateServer.on("/", HTTP_GET, []() {
    updateServer.sendHeader("Connection", "close");
    updateServer.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  updateServer.on("/update", HTTP_POST, []() {
    updateServer.sendHeader("Connection", "close");
    updateServer.send(200, "text/plain", (Update.hasError()) ? F("Fehler") : F("Ok"));
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = updateServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin()) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Erfolgreich: %u\nNeustart...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
    yield();
  });
  updateServer.begin();
  telnetServer.begin();
  //telnetServer.setNoDelay(true);
  DebugOut(true, F("\r\nHTTPUpdateServer bereit! Öffne http://"));
  DebugOut(AppIp.toString().c_str());
  DebugOut(F(":8080/update im browser\r\nTelnet Konsole bereit, zum Verbinden verwende 'telnet "));
  DebugOut(AppIp.toString().c_str());
  DebugOut(F(" 23'\r\n"));
  startWebServers();
}

void dirFiles() {
  File root = LITTLEFS.open("/");
  File file = root.openNextFile();

  while(file){
    telnetClient.print("FILE: ");
    telnetClient.println(file.name());
    file = root.openNextFile();
  }
  file.close();
  root.close();
}

boolean copyFile(String sourceFilename, String destinationFilename) {
  File destFile;
  boolean ret = false;

  if (LITTLEFS.exists(destinationFilename))
    LITTLEFS.remove(destinationFilename);
  binFile = LITTLEFS.open(sourceFilename, "r");
  destFile = LITTLEFS.open(destinationFilename, "a");
  while (binFile.available()) {
    destFile.write(binFile.read());
  }
  destFile.flush();
  if (binFile.size() == binFile.size()) {
    ret = true;
  }
  binFile.close();
  destFile.close();
  return ret;
}

void TelnetDataIn() {
  char in;
  uint8_t Sensor;
  record_t record;
  uint32_t adr;
              
  if (telnetClient.connected()) {
    if (telnetClient.available()) {
      // Daten vom Telnet-Client holen und an den UART weiterleiten
      while (telnetClient.available()) {
        in = telnetClient.read();
        switch (in) {
          case 'Q': {
            telnetClient.println(F("Neustart Gießmonitor..."));
            ESP.restart();
            break;
          }
          case 'F': {
            telnetClient.println(F("Rücksetzen der Konfiguration. Neustart..."));
            for (int i = EEPROM_WLANSSID; i < EEPROMDATEND; ++i) { 
              EEPROM.write(i, 0);
            }
            EEPROM.commit();
            ESP.restart();
            break;
          }
          case 'S': {
            telnetClient.println(F("Sicherungskopie der Grafiktabelle erstellen."));
            if (copyFile(HISTORY_FILENAME, BACKUP_FILENAME)) {
              telnetClient.println(F("Sicherungskopie erfolgreich erstellt."));
            } else {
              telnetClient.println(F("Fehler der Sicherungskopie!"));
            }
            break;
          }
          case 'Z': {
            telnetClient.println(F("Zurückschreiben der Sicherungskopie zur Grafiktabelle."));
            if (LITTLEFS.exists(BACKUP_FILENAME)) {
              if (copyFile(BACKUP_FILENAME, HISTORY_FILENAME)) {
                telnetClient.println(F("Grafiktabelle erfolgreich geschrieben."));
                changeChart = true;
              } else {
                telnetClient.println(F("Fehler beim Schreiben der Grafiktabelle!"));
              }
            } else {
              telnetClient.println(F("Fehler! Keine Sicherungskopie vorhanden."));
            }
            break;
          }
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6': {
            Sensor = in - 0x31; // Char in Intger - 1
            telnetClient.print(F("Lösche alle Messwerte Pflanzensensor "));
            telnetClient.print(Sensor + 1);
            telnetClient.println(F(" aus Grafiktabelle."));
            binFile = LITTLEFS.open(HISTORY_FILENAME, "r+");
            binFile.read(record.ar, RECORD_SIZE);
            adr = RECORD_SIZE + 6 + Sensor; // + 6 ist sizeOf(DayHour) + sizeOf(temperature) + sizeOf(humidity)
            binFile.seek(adr, SeekSet);
            if (record.entries > 0) {
              for (uint16_t i = 0; i < record.entries; i++) {
                binFile.write(0x0);
                binFile.seek(HISTSENS_SIZE - 1, SeekCur); // Cursor nach Schreiben ist um eine Position weiter
              }
            }
            binFile.close();
            changeChart = true;
            break;
          }
          case 'M': {
            history_sensor_t tmpHist;
            File tmpFile;
            telnetClient.println(F("Lösche letzten Messwert der Grafiktabelle aller Pflanzensensoren"));
            binFile = LITTLEFS.open(HISTORY_FILENAME, "r+");
            binFile.read(record.ar, RECORD_SIZE);
            if (record.entries > 0) {
              if (record.entries < MAXHISTORYENTRIES || record.writePos == HISTORYEND - HISTSENS_SIZE) {
                #ifdef DEBUG_TEMP_FILE
                DebugOut(true, "Nur Datenzatz abziehen.\r\n");
                #else
                record.entries--;
                if (record.writePos > RECORD_SIZE) {
                  record.writePos -= HISTSENS_SIZE;
                } 
                binFile.seek(0, SeekSet);
                binFile.write(record.ar, RECORD_SIZE);
                binFile.close();
                #endif
              } else {
                // Wenn an dieser Stelle nur record.writePos und record.entries-- geändert werden, so wird
                // später mit Tabellendarstellung auf Grund if (record.entries < MAXHISTORYENTRIES) adr = RECORD_SIZE;
                // immer falsch mit RECORD_SIZE begonnen!
                if (LITTLEFS.exists(TEMP_FILENAME)) {
                  LITTLEFS.remove(TEMP_FILENAME); // sollte eigentlich nicht vorhanden sein, aber zur Sicherheit
                }
                adr = record.writePos + HISTSENS_SIZE;  // ältester Tabelleneintrag
                binFile.seek(adr, SeekSet);
                tmpFile = LITTLEFS.open(TEMP_FILENAME, "a");
                record.entries--;
                record.writePos = HISTORYEND - (2 * HISTSENS_SIZE);
                tmpFile.write(record.ar, RECORD_SIZE);
                for (uint16_t i = 0; i < record.entries; i++) {
                  binFile.read(tmpHist.ar, HISTSENS_SIZE);
                  tmpFile.write(tmpHist.ar, HISTSENS_SIZE);
                  if (binFile.position() >= HISTORYEND) {
                    adr = RECORD_SIZE;
                    binFile.seek(adr, SeekSet);
                  }
                }
                #ifdef DEBUG_TEMP_FILE
                tmpFile.flush();
                sprintf(debugBuffer, "Größe Temp: %d\r\n", tmpFile.size());
                DebugOut(true, debugBuffer);
                #endif
                tmpFile.close();
                binFile.close();
                dirFiles();
                #ifndef DEBUG_TEMP_FILE
                LITTLEFS.remove(HISTORY_FILENAME);
                LITTLEFS.rename(TEMP_FILENAME, HISTORY_FILENAME);
                #endif
              }
              changeChart = true;
            }
            break;
          }
          case 'L': {
            telnetClient.println(F("Lösche Grafiktabelle aller Pflanzensensoren"));
            record.writePos = 0;
            record.entries = 0;
            binFile = LITTLEFS.open(HISTORY_FILENAME, "w");
            binFile.write(record.ar, RECORD_SIZE);
            binFile.close();
            changeChart = true;
            break;
          }
          case '?': {
            telnetClient.println(F("'Q' Neustart Gießmonitor...\r\n'F' Rücksetzen der Konfiguration. Neustart...\r\n'S' Sicherungskopie der Grafiktabelle erstellen.\r\n'Z' Zurückschreiben der Sicherungskopie zur Grafiktabelle.\r\n'1' bis '6' Löscht alle Messwerte des Pflanzensensors X\r\n'M' Lösche letzten Messwert der Grafiktabelle aller Pflanzensensoren\r\n'L' Lösche Grafiktabelle aller Pflanzensensoren\r\n"));
            uint8_t i = 150;
            while (i-- > 0) {
              delay(20);
              yield();
              //timerWrite(timer, 0); //timer rücksetzen watchdog
            }
            break;
          }
          case 'X': {
            break;
          }
        }
      }
      telnetClient.flush();
    }
  } else {
    telnetClient.stop(); // Client getrennt ... Sitzung beenden 
  }
}

void EEPROMsetDefaultData() {
  // möglichst nur bei leeren EEPROM ausführen
  boolean fsOk = false;

  Serial.println("EEPROM wird initialisiert");
  for (int i = 0; i < EEPROM_ALLOCATE_MEM; i++) {
    EEPROM.write(i, 0x0);
  }
  EEPROM.write(EEPROM_OFFSETTZ, 1);
  EEPROM.write(EEPR_SHOWGWM, true);
  EEPROM.write(EEPR_HYSTERESE, 15);
  EEPROM.put(EEPR_LED_DRY, 0x0000FF);
  EEPROM.put(EEPR_LED_WARN,0x0096FF);
  EEPROM.put(EEPR_LED_HUMID, 0x009600);
  EEPROM.put(EEPR_LED_NOSENSOR, 0xFF0000);
  EEPROM.commit();

  fsOk = LITTLEFS.begin(true);
  if (!fsOk) { // 2. Versuch => mount geht manchmal schief
    fsOk = LITTLEFS.begin(true);
  }
  if (fsOk) {
    LITTLEFS.end();
  } else LITTLEFS.format();
}

void WiFiEvent(WiFiEvent_t event, system_event_info_t info) {
  switch (event) {
    case SYSTEM_EVENT_STA_START:
      Serial.println(F("Station Mode gestartet"));
      break;
    case SYSTEM_EVENT_STA_GOT_IP: {
      Serial.println("Verbunden mit :" + String(WiFi.SSID()));
      Serial.print(F("Bekam IP: "));
      Serial.println(WiFi.localIP());
      }
      break;
    //case SYSTEM_EVENT_AP_STADISCONNECTED: // nur zur Vollständigkeit
    //  break;
    case SYSTEM_EVENT_STA_DISCONNECTED: {
      Serial.println(F("Vom WLAN getrennt, versuche Wiederverbindung"));
      WiFi.reconnect();
      }
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      Serial.println("WPS erfolgreich, stoppe WPS und verbinde mit: " + String(WiFi.SSID()));
      break;
    default: break;
  }
}

void initializeWiFi() {
  wifi_init_config_t cfg;

  boolean manual_network = EEPROM.read(EEPROM_MANUALNETWORK);
  IPAddress manual_ip = EEPROM.readLong(EEPROM_IPADDR);
  IPAddress manual_gw = EEPROM.readLong(EEPROM_GWADDR);
  IPAddress manual_subnet = EEPROM.readLong(EEPROM_SNADDR);
  IPAddress manual_dns = EEPROM.readLong(EEPROM_DNSADDR);

  uint8_t checkSSID = EEPROM.read(EEPROM_WLANSSID);
  if (checkSSID != 0 && checkSSID != 255) {
    EEPROM.get(EEPROM_WLANSSID, wlanssid);
    EEPROM.get(EEPROM_WLANKEY, wlankey);
  } else {
    strcpy (wlanssid, SSID_WLAN);
    strcpy (wlankey, KEY_WLAN);
    EEPROM.put(EEPROM_WLANSSID, wlanssid);
    EEPROM.put(EEPROM_WLANKEY, wlankey);
    EEPROM.commit();
  }

  WiFi.persistent(false);
  //fixWifiPersistencyFlag();
  cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.nvs_enable = 0;

  WiFi.disconnect();
  WiFi.setHostname("PfSensor");
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  /* WiFi.setSleep(WIFI_PS_MIN_MODEM); // Der standardmäßige Modem-Ruhemodus ist WIFI_PS_MIN_MODEM.*/

  if (checkSSID != 0 && checkSSID != 0xFF) {
    if (manual_network == true) {
      WiFi.config(manual_ip, manual_gw, manual_subnet, manual_dns);
    }
  }
  WiFi.begin(wlanssid, wlankey);
 
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 500) {
    delay(20);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();  
    if (checkSSID != 0 && checkSSID != 255) {
      wifi_failed = true;
    }
    WiFi.softAP(wlanssid, wlankey);
    AppIp = WiFi.softAPIP();
  } else {
    if (manual_network == true) {
      boolean ping_gw = false;
      uint8_t ping_counter = 0;
      while (ping_gw == false && ping_counter < 10) {
        ping_gw = Ping.ping(manual_gw);
        ping_counter++;
        delay(1);
      }
      if (ping_gw == false) {
        WiFi.disconnect();
        delay(2000);
        WiFi.softAP(wlanssid, wlankey);
        AppIp = WiFi.softAPIP();
        wifi_failed = true;
      } else {
        AppIp = WiFi.localIP();
      }
    } else {
      AppIp = WiFi.localIP();
    }
  }
} // initializeWiFi

void setup() {
  String firstInit;
  Preferences preferences;
  char ntpServer[EEPROM_OFFSETTZ-EEPROM_NTPSERVER] = { 0 };

  Serial.begin(115200);   // initialize serial communication at 115200 bits per second:
  starttime = millis() + STARTTIME;
  EEPROM.begin(EEPROM_ALLOCATE_MEM);
  preferences.begin("Pfls", false);
  //preferences.remove("firstInit"); 
  //preferences.clear();
  firstInit = preferences.getString("firstInit");
  Serial.println(firstInit);
  if (firstInit != FIRST_INIT_KEY) {
    preferences.putString("firstInit", FIRST_INIT_KEY);
    Serial.println("Initialisierung");
    // EEPROM mit 0x0 initialisieren undStandartwerte für Zeitzone gleitenden MW und LED setzen,
    // LITTLEFS.begin(true) gegebenenfalls formatieren
    //EEPROMsetDefaultData();
  } else Serial.println("War schon gestartet.");
  preferences.end();

  #ifdef EEPROMCLEAR
    /* Clear EEPROM */
    Serial.println(F("Bereinige EEPROM...")); // Dateien für Historie werden nicht gelöscht!
    for (int i = EEPROM_WLANSSID; i < EEPROMDATEND; ++i) { 
    //for (int i = EEPROM_WLANSSID; i < EEPROM_ALLOCATE_MEM; ++i) { 
      EEPROM.write(i, 0x00);
    }
    Serial.println(F("Fertig"));
    EEPROM.commit();
  #endif

  uint16_t check_credentials = EEPROM.read(EEPROM_HTTPUSER);
  if (check_credentials != 0 && check_credentials != 255) {
    EEPROM.get(EEPROM_HTTPUSER, http_user);
    EEPROM.get(EEPROM_HTTPPASS, http_pass);
  } else {
    strcpy (http_user, USER_HTTP);
    strcpy (http_pass, PASS_HTTP);
    EEPROM.put(EEPROM_HTTPUSER, http_user);
    EEPROM.put(EEPROM_HTTPPASS, http_pass);
    EEPROM.commit();
  }

  check_credentials = EEPROM.read(EEPROM_NTPSERVER);
  if (check_credentials != 0 && check_credentials != 255) {
    EEPROM.get(EEPROM_NTPSERVER, ntpServer);
  } else {
    strcpy (ntpServer, "pool.ntp.org");
    EEPROM.put(EEPROM_NTPSERVER, ntpServer);
    EEPROM.commit();
  }

  initializeWiFi();
  //StartUpdateServer();

    // watchdog
  //timer = timerBegin(0, 80, true);                  // timer 0, div 80
  //timerAttachInterrupt(timer, &resetModule, true);  // callback hinzufügen
  //timerAlarmWrite(timer, wdtTimeout * 1000, false); // setze Zeit in us
  //timerAlarmEnable(timer);                          // aktiviere interrupt

  tzOffset = EEPROM.read(EEPROM_OFFSETTZ);
  configTime(tzOffset * 3600, daylightOffset_sec, ntpServer);

  swShowRollingMean = EEPROM.read(EEPR_SHOWGWM);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  ledcSetup(PWMledChannelA, PWMfreq, PWMresolution);
  ledcSetup(PWMledChannelB, PWMfreq, PWMresolution);
  ledcSetup(PWMledChannelC, PWMfreq, PWMresolution);
  ledcAttachPin(LED_RED, PWMledChannelA);   // verbinde den PWM-Kanal zum steuernden GPIO
  ledcAttachPin(LED_BLUE, PWMledChannelB);
  ledcAttachPin(LED_GREEN, PWMledChannelC);
  led_rgb.rd = 255;
  led_rgb.gn = 255;
  led_rgb.bl = 255;
  SetLed();
  DebugOut(F("\r\n"));
  DebugOut(true, F("Systemkonfiguration:"));

  #ifdef DHT_SENSOR
  dht.begin();
  #endif
  #ifdef BME280_SENSOR
  hts.begin(i2caddr);
  #endif
  if (Run_HTSensor() != SensorNotDetected) {
    DebugOut(F(" 1 HT-Sensor"));
  }

  led_rgb.rd = 0;
  led_rgb.gn = 0;
  led_rgb.bl = 0;
  SetLed();
  TimeFullHour();
  initHistoryFile();
  Run_MoistureSensors();
  StartUpdateServer();
} // setup()

// Main Loop
void loop() {
  // prüfen, ob es neue Telnet-Clients gibt
  if (telnetServer.hasClient()) {
    // freien/getrennten Platz finden
    if (!telnetClient) {
      telnetClient = telnetServer.available(); // neuer Telnet-Client
      telnetClient.flush(); // Eingabepuffer löschen
      telnetClient.println();
      telnetClient.print(F("Freier dynamischer Speicher: "));
      telnetClient.println(String(ESP.getFreeHeap()));
      telnetClient.print(F("Betriebszeit: "));
      telnetClient.println(getUptime());
      telnetClient.print(F("Version: "));
      telnetClient.print(version);
      telnetClient.print(F(" / "));
      telnetClient.print(today);
      telnetClient.print(F(" "));
      telnetClient.println(tstamp);
      telnetClient.print(F("Letzter Grund für Rücksetzen: "));
      // https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
      telnetClient.println(rtc_get_reset_reason(0)); // 12=0xc Software CPU Reset
      telnetClient.print(F("Letzter WiFi status: "));
      telnetClient.println(wifiStatus);
      /*uint8_t i = 150;
      while (i-- > 0) {
        delay(20);
        yield();
        timerWrite(timer, 0); //timer rücksetzen watchdog
      }*/
    }
  }

  // Überprüfung des Clients auf eingehende Telnet-Daten
  if (telnetClient) {
    TelnetDataIn();
  }

  if (pollIntervall < millis()) { // nach Setup ist pollIntervall=0 und wird sofort ausgeführt
    pollIntervall = millis() + POLL_INTERVALL;
    boolean clearMean = TimeFullHour();
    Run_MoistureSensors();
    if (clearMean) {
      pollMean = millis() + POLL_MEAN; // zur Stunde synchronisieren
      for (int i = 0; i < MaxSensors; i++) { // stündlich nur einfacher Mittelwert
        clearMeanData(i);
      }
      clearhtMeanData();
    }
  }

  if (HTMeasure.Status  != SensorNotDetected) {
    Run_HTSensor();
  }

  webServer.handleClient();
  updateServer.handleClient();

  if (wifi_failed == true && starttime < millis()) { // "Neustart für neues WiFi.. oder bei einigen Änderungen Konfiguration
    ESP.restart();
  }

  //timerWrite(timer, 0); //timer rücksetzen watchdog
}
