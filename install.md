####Installation der Software
Eine Anleitung findet ihr u.a. auf https://dronebotworkshop.com/esp32-intro/
Mittels USB-Kabel ist das ESP32 Board mit Eurem Computer zu verbinden.
Unter Werkzeuge der Arduino IDE sind folgende Parameter einzustellen:
* Board: "LOLIN D32"
* Upload Speed: "921600"
* Flash Frequency: "80MHz"
* Partition Scheme: "Standard"
* Core Debug Level: "Keine"
* Port :

Gieshmon.ino und giessmon.h müssen sich im selben Ordner befinden. Die Deklarationen in der giessmon.h sind entsprechend eurer WLAN-Router-Einstellungen anzupassen.
\#define SSID_WLAN  "Meine WLAN-SSID"
\#define KEY_WLAN   "Mein WLAN-Schlüssel"

Darunter könnt ihr auch Benutzername und Kennwort für den Webserverzugang anpassen.
Diese Änderungen sind aber auch noch auf der Konfigurationsseite „Pflanzen Gießmonitor“ möglich. Per USB-Kabel ist nun das esp32-Modul mit eurem Computer zu verbinden. Vergesst bitte nicht unter Werkzeuge den USB-Port anzugeben, an welchen das esp32-Modul angeschlossen ist.
Wählt auf der Arduino-IDE „Sketch“ => „hochladen“. Nach erfolgreicher Kompilierung und hochladen der Software ist „Pflanzen Gießmonitor“ über http://IP-AdresseESP32 erreichbar.
