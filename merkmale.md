####Merkmale
-	Die Darstellung der Messwerte erfolgt ausschließlich auf auswählbaren Webseiten (http://IP-AdresseESP32).
-	Raumsensor für Temperatur und Luftfeuchte ist fest integriert.
-	Auf die Messung der Lichtstärke habe ich verzichtet.
-	Feuchtigkeitssensoren können ohne Neustart des esp32-Moduls jederzeit gesteckt, bzw. entfernt werden.
-	Änderungen der Einstellungen und Kalibrierungen sind ohne erneutes kompilieren möglich.
-	Grafische Darstellung ausgewählter Pflanzensensoren plus zusätzliches einblenden von Raumtemperatur und relativer Luftfeuchte in einem maximalen Zeitraum von 3 Wochen. 
-	Die grafische Darstellung der Messwerte können mittels gleitenden Mittelwert geglättet werden. Mit der einstellbaren Hysterese kann man den Zeitpunkt des Gießens hervorheben.
-	Die allgemeine Pflanzensensorbezeichnung kann durch einen kurzen Pflanzennamen einfach ersetzt werden.
-	http://IP-AdresseESP32/dump listet tabellarisch die gespeicherten Werte/Variablen.
-	Mit telnet IP-AdresseESP32 kann man sich, bei Auskommentierung von #define DEBUG_TELNET, wesentliche Informationen während der Programmlaufzeit ausgeben lassen.
Zusätzlich können folgende Kommandos ausgeführt werden:
?
'Q' Neustart Gießmonitor...
'F' Rücksetzen der Konfiguration. Neustart...
'S' Sicherungskopie der Grafiktabelle erstellen.
'Z' Zurückschreiben der Sicherungskopie zur Grafiktabelle.
'1' bis '6' Löscht alle Messwerte des Pflanzensensors X
'M' Lösche letzten Messwert der Grafiktabelle aller Pflanzensensoren
'L' Lösche Grafiktabelle aller Pflanzensensoren
-	http://IP-AdresseESP32:8080 ermöglicht Softwareupdate über WLAN