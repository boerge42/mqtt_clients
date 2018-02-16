# MQTT-Clients
(Uwe Berger; 2017)

Eine Sammlung diverser MQTT-Clients unter Verwendung von libmosquitto:

## mqtt_subscribe
Ein simples Programm, welches einen MQTT-Tobic abonnieren und dessen 
Inhalte anzeigen kann.

## mqtt2sqlite
Client-Server-Konstrukt, welches zum einen MQTT-Telegramme (im JSON-
Format) abboniert, diese in entsprechende SQL-Befehle umsetzt und (via 
TCP/IP) an ein weiteres Script (hier sqlite_server.tcl) sendet, welches 
die eigentlichen DB-Operationen ausfuehrt. Im Fehlerfall (SQL-Befehl 
schlaegt fehl) wird  SQL-Befehl in einer FIFO-Queue abgelegt...

## myweather2mqtt
Werte (abgelegt in Dateien im libconfig-Format) meiner Wetterstation 
auslesen und via MQTT publizieren.

## strtok_mqtt_test
Eine Fingeruebung mit dem Ziel, eine bestimmt formatierte MQTT-Message auseinander
nehmen zu koennen.

## sysinfo2mqtt
Diverse Systeminformationen des Rechners zu einem MQTT-Boker senden. Funktioniert 
natuerlich nur unter Linux ;-)...
  
  
---------  
Have fun!
