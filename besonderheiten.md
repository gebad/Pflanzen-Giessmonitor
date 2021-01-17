####Besonderheiten des Programms
Die kapazitiven Feuchtigkeitssensoren v1.2 haben eine extrem große Streuung vom analogen Spannungs-/Messwert zur vorhandenen Feuchtigkeit. Weiterhin werden die Messwerte zusätzlich von der Versorgungsspannung der genannten Sensoren beeinflusst. 
Aus diesen Grund wird aller 5 Minuten eine Messung zur Bildung eines einfachen Mittelwertes herangezogen, bzw. summiert. Nach Vollendung einer Stunde wird der Mittelwert aus maximalen 12 Messwerten (MEASURE_PER_HOUR) berechnet und in einer Tabelle gespeichert.
Ausnahme, der Sensor wurde irgendwann innerhalb dieser Zeit neu angeschlossen oder der Feuchtigkeitswert ist sprunghaft angestiegen (Einstellwert Hysterese). In so einen Fall erfolgt die Mittelwertbildung ab dieser Zeit.
Die Tabelle der Messwerte ist in der Datei history.bin gespeichert und wurde als sogenannter Ringspeicher programmiert. Mit bis zu 552 Datensätzen (MAXHISTORYENTRIES) vergrößert sich die Datei. Danach wird der älteste Datensatz durch den Aktuellen überschrieben.
Hierzu muss der Schreibcursor innerhalb des Datenstroms frei positionierbar sein ohne die Datei zu löschen LITTLEFS.open(HISTORY_FILENAME, "r+"). Es ist auch nicht einfach möglich irgendwelche Zahlentypen oder Strukturen, wie bei EEPROM.put, in die Datei direkt zu schreiben oder auszulesen. Hier hilft union weiter. Mit die dieser Deklaration können unterschiedliche definierte typen auf den gleichen Speicherplatz geschrieben werden z.B.:
```
struct history_sensor_t{
  union {
    struct {
      uint16_t DayHour;
      int16_t temperature;
      int16_t humidity;
      int8_t  humidPercent[MaxSensors]; // nur 0 bis 100% möglich;
    };
    uint8_t ar[12]; // muss feste Größe sein!
  };
};
history_sensor_t HistorySensor;
```
Das Array ar beginnt an gleicher Stelle, wie struct. Man könnte also auch sagen, das Array überlagert struct.

Damit kann man einfach ab aktueller Cursorposition Werte unterschiedlichen Variablen in einen Datenstrom überschreiben:
```
  binFile.seek(record.writePos, SeekSet);
  binFile.write(HistorySensor.ar, HISTSENS_SIZE);
```
union eignet sich auch sehr gut für einen gelesenen Hexwert z.B. in float oder signed Integer zu konvertieren.

handle_renewtable() bereitet die Daten der Tabelle für die Darstellung mit google linechart auf. Diese werden dann in richtiger zeitliche Reihenfolge in einem 2-diminsionalen Array zur Verfügung gestellt. Optional wird in dieser Funktion der gleitende Mittelwert berechnet. Dieser glättet die grafisch dargestellten Kurven.
