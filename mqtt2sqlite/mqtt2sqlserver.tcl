#
# mqtt2sqliteserver.tcl
# =====================
#   Uwe Berger; 2018
#
#
# Generiert aus dem (JSON-)Inhalt einer (abbonierten) MQTT-Nachricht 
# entsprechende SQL-Befehle und speichert diese in einer Queue. Diese
# Queue wird, der Reihenfolge nach, an einen Empfaenger (hier 
# konfigurierter Host und entspr. Port (siehe gvar); ...z.B. Script 
# sqlite_server.tcl) gesendet, welches die eigentlichen # DB-Aktionen 
# ausfuehrt. Tritt dort ein Fehler auf bzw. ist der Empfaenger
# nicht erreichbar, wird der entsprechende Eintrag in der Queue nicht 
# geloescht und immer wieder gesendet...
#
# Konventionen:
# -------------
# * MQTT-Nachrichten sind im JSON-Format
# * JSON-Element 'node_type' bestimmt den Tabellennamen
# * Spaltennamen der Tabelle entsprechen dem gleichnamigen JSON-Element
#
#
# ---------
# Have fun!
#
#

package require json
package require sqlite3
source  mqtt.tcl

set gvar(port)		8266
set gvar(host)		localhost

#
# SQL-Tabellen (Spaltenname Type)
#
set tab(dht22) {
			node_name text
	 		unixtime integer
	 		temperature real
	 		humidity real
	 		heap integer
}

set tab(bme280) {
			node_name text
	 		unixtime integer
	 		temperature real
	 		humidity real
	 		pressure_rel real
	 		drew_point real
	 		heap integer
}

set tab(dummy) {
			col1 text
}

# SQL-Queue definieren/setzen
set sql_queue {}

# **************************************************************
proc sql_queue_fill {s} {
	global sql_queue	
	set $sql_queue [lappend sql_queue $s]
}

# **************************************************************
proc send_sqlite_cmd {cmd} {
	global gvar
	# kann Verbindung zum Server aufgebaut werden?
	if {[catch {set chan [socket $gvar(host) $gvar(port)]}]} {
		# kein Server erreichbar, also -1 und Schluss
		return [list -1 ""]
	} else {
		# Verbindung aufgebaut, SQL-Kommando senden und
		# Ergebnis zurueckgeben
		puts $chan $cmd
		flush $chan
		set result [gets $chan]
		if {[eof $chan]} {
			set result [list -1 ""]
		}
		close $chan
		return $result
	}
}

# **************************************************************
proc sql_queue_work {delay} {
	global sql_queue
	set sql_error 0
	if {[llength $sql_queue]} {
		set sql [lindex $sql_queue 0]
		set sql_error [lindex [send_sqlite_cmd $sql] 0]
		if {$sql_error} {
			puts "SQL-Error: $sql_error ($sql)"				
		} else {
			set sql_queue [lreplace $sql_queue 0 0]
			puts $sql
		}
	}
	after $delay [list sql_queue_work $delay]
}

# **************************************************************
proc callback_mqtt_sub {t p} {
	global tab db_name
	# json in ein named array umwandeln (array --> v)
	array set v [json::json2dict $p]
	# create table zusammenbauen...
	set c ""
	set sql "create table if not exists $v(node_type) ("
	foreach {col type} $tab($v(node_type)) {
		set sql "$sql$c $col $type"
		set c ","
	}
	set sql "$sql)"	
	# ...und in die SQL-Queue eintragen
	sql_queue_fill $sql
	
	# insert into zusammenbauen...
	set c ""
	set sql "replace into $v(node_type) values ("
	foreach {col type} $tab($v(node_type)) {
		if {$type == "text"} {set a "'"} else {set a ""}
		set sql "$sql$c$a$v($col)$a"
		set c ","	
	}
	set sql "$sql)"
	# ...SQL-Queue dito
	sql_queue_fill $sql
}

# **************************************************************
# **************************************************************
# **************************************************************

# mqtt log str {puts $str}

# MQTT-Kanaele abbonieren und entsprechende Callback-Funktion etablieren
set client [mqtt new]
$client connect tcl-ding dockstar
$client subscribe {sensors/+/json} callback_mqtt_sub 0

# SQL-Kommando-Queue-Verarbeitung starten
sql_queue_work 1000

vwait forever
