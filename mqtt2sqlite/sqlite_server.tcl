#!/usr/bin/tcl
#
# sqlite_server.tcl
# =================
# Uwe Berger; 2015
#
# SQLite-Server; empfaengt ein SQL-Kommando ueber das Netzwerk ($port) 
# und fuehrt dieses auf der angebenen SQLite-DB ($db_name) aus.
#
# Als Ergebnis wird eine Tcl-Liste zurueckgesendet:
#   * 1.Listenelement: * 0 alles ok
#                      * >0 sqlite-Error
#                      * -2 Verbindung durch andere Vebindung blockiert 
#   * 2.Listenelement: Ergebnis des SQL-Kommandos als weitere Tcl-
#                      Liste; sinnvollerweise nur bei einem select-
#                      Befehl gefuellt.
#
# Wenn $debug != 0, dann wird der Server etwas gespraechiger...
#
# ---------
# Have fun!
#
#

package require sqlite3

set db_name test.sqlite

set port 8266

set current_chan ""
set debug 1
set error_count 0
set error_max 200
set sname $argv0

# *********************************************
proc puts_debug {txt} {
	global debug
	if {$debug} {
		puts $txt
	}
}

# *********************************************
proc accept {chan addr port} {
	global current_chan
	#puts_debug "current: $current_chan new: $chan"
	# gibt es momentan schon eine Verbindung?
	if {$current_chan ne ""} {
		# ...ja, dann mit Fehlermeldung (-2) an Client beenden
		puts $chan [list -2 ""]
		flush $chan
		return
	}
	# aktuelle Verbindung merken
	set current_chan $chan
	# wenn Daten vom Client ankommen, dann exec_sqlite ausfuehren
	fileevent $chan readable [list exec_sqlite $chan]
	# ...fuer Testzwecke
}

# *********************************************
proc restart_sqlite_connection {} {
	global db_name
	# ...catch, falls db-Objekt (noch) nicht existiert
	catch {db close}
	sqlite3 db $db_name
	db timeout 5000	
}

 
# *********************************************
proc exec_sqlite {chan} {
	global db_name current_chan error_count error_max sname
	fileevent $chan readable {}
	set result ""
	set catch_error 0
	# DB oeffnen, SQL-Kommando ueber Socket lesen und ausfuehren
	# sqlite3 db $db_name
	# db timeout 5000
	set sql_cmd [gets $chan]
	puts_debug "server rx: $sql_cmd"
	if {[catch {set result [db eval $sql_cmd]}]} {
		set catch_error 1
	}
	# Ergebnis als Tcl-Liste (siehe Beschreibung) zuruecksenden
	set sql_error [db errorcode]
#	puts $chan [list [db errorcode] $result]
	puts $chan [list $sql_error $result]
	flush $chan
	# bei SQL-Fehler oder bei catch_error error_count hochzaehlen...
	if {($sql_error != 0) || ($catch_error != 0)} {
		incr error_count
		puts_debug "error_count: $error_count ($catch_error , $sql_error)"
	} else {
		set error_count 0
	}
	# aktuell bearbeitete Verbindung zuruecksetzen
	set current_chan ""
	close $chan
	# wenn zuviele Fehler aufgetreten sind, einen neuen Server
	# starten und alten schliessen
	if {$error_count > $error_max} {
		puts_debug [exec tclsh $sname &]
		exit
	}
}

# *********************************************
# *********************************************
# *********************************************

# DB oeffnen
sqlite3 db $db_name
db timeout 5000

# Server-Socket definieren
socket -server accept $port

# "ewig" an dieser Stelle warten
vwait forever
