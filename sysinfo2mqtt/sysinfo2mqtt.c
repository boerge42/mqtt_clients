/*
*	  sysinfo2mqtt
*	================
*	Uwe Berger; 2017
*
* Sysinfo des Rechners zu einem MQTT-Boker senden.
* Funktioniert natuerlich nur unter Linux ;-)...
*
* Aufruf:
* -------
* sysinfo2mqtt [-h <mqtt-host>] [-p <mqtt-port>]
* 
* MQTT-Topics:
* ------------
* <hostname>/sysinfo/uptime
* <hostname>/sysinfo/unixtime
* <hostname>/sysinfo/readable_timestamp
* <hostname>/sysinfo/load1m
* <hostname>/sysinfo/load5m
* <hostname>/sysinfo/load15m
* <hostname>/sysinfo/freeram
* <hostname>/sysinfo/shareram
* <hostname>/sysinfo/bufferram
* <hostname>/sysinfo/totalram
* <hostname>/sysinfo/freeswap
* <hostname>/sysinfo/totalswap
* <hostname>/sysinfo/processes
*
* ...sowie als JSON-String, z.B. (nicht sinnvoll formatiert ;-)...):
* samsung/sysinfo/json 
* { "hostname": "samsung", "uptime": "0 days, 2:21:32", 
* "load": [ "0.26", "0.33", "0.42" ], 
* "ram": { "free": "396684", "share": "141964", "buffer": "166340", 
* "total": "2051736" }, "swap": { "free": "2988028", "total": "2988028" }, 
* "processes": "470", "time": { "unix": "1508349461", 
* "readable": "2017\/09\/18 19:57:41" } }
* 
* ---------
* Have fun!
*
*/

#include <sys/sysinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <mosquitto.h>
#include <json/json.h>

// MQTT-Defaults
#define MQTT_HOST 		"dockstar"
#define MQTT_PORT 		1883
#define MQTT_RETAIN		false
#define MQTT_QOS		1
#define MQTT_KEEPALIVE 	120

// sysinfo...
#define SEC_MINUTE					(60)
#define SEC_HOUR 					(SEC_MINUTE * 60)
#define SEC_DAY						(SEC_HOUR * 24)
#define ONE_KBYTE					1024
#define LINUX_SYSINFO_LOADS_SCALE	(double)65536

struct mosquitto *mosq = NULL;
unsigned char debug = 0; 
bool disconnect = false;

// ************************************************
int mosquitto_error_handling(int error)
{
	switch(error)
    {
        case MOSQ_ERR_SUCCESS:
			return 0;
            break;
        case MOSQ_ERR_INVAL:
        case MOSQ_ERR_NOMEM:
        case MOSQ_ERR_NO_CONN:
        case MOSQ_ERR_PROTOCOL:
        case MOSQ_ERR_PAYLOAD_SIZE:
		case MOSQ_ERR_CONN_LOST:
		case MOSQ_ERR_NOT_SUPPORTED:
		case MOSQ_ERR_ERRNO:
				fprintf(stderr, "Mosquitto-Error(%i): %s\n", error, mosquitto_strerror(errno));
				exit(EXIT_FAILURE);
				break;
    }
	return 0;
}

// ************************************************
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	// ...
}

// ************************************************
void my_publish_callback(struct mosquitto *mosq, void *userdata, int mid)
{
	// im Debug-Mode gesendete Message-ID ausgeben
	if (debug) printf("publish msgID: %i\n", mid);
	if (disconnect) mosquitto_disconnect(mosq);
}

// ************************************************
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	// im Debug-Mode Mosquitto-Log-Message ausgeben
	if (debug) printf("%s\n", str);
}

// ************************************************
int publish2mqtt(char *hostname, char *sub_topic, char *message, int qos, bool retain)
{
	char topic[1024];

	// Topic zusammenbauen und Message versenden
	sprintf(topic, "%s/sysinfo/%s", hostname, sub_topic);
    mosquitto_error_handling( mosquitto_publish(mosq, NULL, topic, strlen(message), message, qos, retain));

	// Message rausschicken
	//mosquitto_error_handling(mosquitto_loop(mosq, 10000, 1));

    return 0;
}

// ************************************************
// ************************************************
// ************************************************
int main(int argc, char **argv)
{
	char mqtt_host[50]	= MQTT_HOST;
	int  mqtt_port    	= MQTT_PORT;
	bool mqtt_retain	= MQTT_RETAIN;
	int  mqtt_qos		= MQTT_QOS;
    char s[255] 		= "";
    char hostname[255] 	= "";
    struct sysinfo si;
	struct tm *tmnow;
	time_t tnow;
	int c;
	int major, minor, revision;
	
	json_object *jobj = json_object_new_object();
	json_object *jobj1 = json_object_new_object();
	json_object *jstr;
	json_object *jarray = json_object_new_array();
	
	// Aufrufparameter auslesen/verarbeiten
	while ((c=getopt(argc, argv, "h:p:?drq:")) != -1) {
		switch (c) {
			case 'h':
				if (strlen(optarg) >= sizeof mqtt_host) {
					puts("hostname too long!");
					exit(EXIT_FAILURE);
				} else {
					strcpy(mqtt_host, optarg);
				}
				break;
			case 'p':
				mqtt_port = atoi(optarg);
				break;
			case 'q':
				mqtt_qos = atoi(optarg);
				if ((mqtt_qos < 0) || (mqtt_qos > 2)) {
					puts("-q <n>: n must between 0...2");
					exit(EXIT_FAILURE);			
				}
				break;
			case 'r':
				mqtt_retain=true;
				break;
			case 'd':
				debug=1;
				break;
			case '?':
				puts("sysinfo2mqtt [-h <mqtt-host>] [-p <mqtt-port>] [-d] [-r] [-q <0...2>]");
				puts("Send sysinfo to a mqtt-broker; Uwe Berger, 2017");
				exit(0);
				break;
		}
	}
	
    // (eigenen) Hostname bestimmen
    gethostname(hostname, sizeof(hostname));
	jstr = json_object_new_string(hostname);
	json_object_object_add(jobj, "hostname", jstr);

	// Init Mosquitto-Lib...
    mosquitto_lib_init();

	// im Debug-Mode Version von libmosquitto ausgeben
	if (debug) {
		mosquitto_lib_version(&major, &minor, &revision);
		printf("libmosquitto-Version: %i.%i.%i\n", major, minor, revision);
	}
	
    // einen Mosquitto-Client erzeugen
    mosq = mosquitto_new(hostname, true, NULL);
    if( mosq == NULL )
    {
        switch(errno){
            case ENOMEM:
                fprintf(stderr, "Error: Out of memory.\n");
                break;
            case EINVAL:
                fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
                break;
        }
        mosquitto_lib_cleanup();
        exit(EXIT_FAILURE);
    }
    
    mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);
   
   
	// mit MQTT-Broker verbinden
    mosquitto_error_handling(mosquitto_connect(mosq, mqtt_host, mqtt_port, MQTT_KEEPALIVE));

    // Systeminformationen zusammensuchen und publizieren
    sysinfo(&si);

    sprintf (s, "%ld days, %ld:%02ld:%02ld", 
				si.uptime / SEC_DAY, 
				(si.uptime % SEC_DAY) / SEC_HOUR, 
				(si.uptime % SEC_HOUR) / SEC_MINUTE, 
				 si.uptime % SEC_MINUTE);
	publish2mqtt(hostname, "uptime", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj, "uptime", jstr);
	
    sprintf (s, "%.2f", si.loads[0] / LINUX_SYSINFO_LOADS_SCALE);
	publish2mqtt(hostname, "load1m", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_array_add(jarray, jstr);

    sprintf (s, "%.2f", si.loads[1] / LINUX_SYSINFO_LOADS_SCALE);
	publish2mqtt(hostname, "load5m", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_array_add(jarray, jstr);

    sprintf (s, "%.2f", si.loads[2] / LINUX_SYSINFO_LOADS_SCALE);
	publish2mqtt(hostname, "load15m", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_array_add(jarray, jstr);
	json_object_object_add(jobj, "load", jarray);

	jarray = json_object_new_array();

	
	sprintf (s, "%lu", si.freeram * si.mem_unit / ONE_KBYTE);
	publish2mqtt(hostname, "freeram", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "free", jstr);
	json_object_array_add(jarray, jobj1);

	sprintf (s, "%lu", si.sharedram * si.mem_unit / ONE_KBYTE);
	publish2mqtt(hostname, "shareram", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "share", jstr);
	json_object_array_add(jarray, jobj1);

	sprintf (s, "%lu", si.bufferram * si.mem_unit / ONE_KBYTE);
	publish2mqtt(hostname, "bufferram", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "buffer", jstr);
	json_object_array_add(jarray, jobj1);

	sprintf (s, "%lu", si.totalram * si.mem_unit / ONE_KBYTE);
	publish2mqtt(hostname, "totalram", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "total", jstr);
	json_object_array_add(jarray, jobj1);

	json_object_object_add(jobj, "ram", jobj1);
	jarray = json_object_new_array();
	jobj1 = json_object_new_object();

	sprintf (s, "%lu", si.freeswap * si.mem_unit / ONE_KBYTE);
	publish2mqtt(hostname, "freeswap", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "free", jstr);
	json_object_array_add(jarray, jobj1);

	sprintf (s, "%lu", si.totalswap * si.mem_unit / ONE_KBYTE);
	publish2mqtt(hostname, "totalswap", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "total", jstr);
	json_object_array_add(jarray, jobj1);

	json_object_object_add(jobj, "swap", jobj1);
	jarray = json_object_new_array();
	jobj1 = json_object_new_object();

	sprintf (s, "%u", si.procs);
	publish2mqtt(hostname, "processes", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj, "processes", jstr);
	
	// Timestamp ermitteln und publizieren...
	sprintf(s, "%lu", time(NULL));
	publish2mqtt(hostname, "unixtime", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "unix", jstr);
	json_object_array_add(jarray, jobj1);

	time(&tnow);
	tmnow = localtime(&tnow);
	sprintf(s, "%d/%02d/%02d %02d:%02d:%02d", 
				tmnow->tm_year+1900,
				tmnow->tm_mon+1, 
				tmnow->tm_mday,
				tmnow->tm_hour,
				tmnow->tm_min,
				tmnow->tm_sec
			);
	
	publish2mqtt(hostname, "readable_timestamp", s, mqtt_qos, mqtt_retain);
	jstr = json_object_new_string(s);
	json_object_object_add(jobj1, "readable", jstr);
	json_object_array_add(jarray, jobj1);
	
	json_object_object_add(jobj, "time", jobj1);

	disconnect = true; 
	publish2mqtt(hostname, "json", (char *)json_object_to_json_string(jobj), mqtt_qos, mqtt_retain);	
	
	// ...hmm, warum werden sonst nicht alle Nachrichten gesendet?
   	usleep(10000);
   	//mosquitto_error_handling(mosquitto_loop_forever(mosq, 100000, 1));

    // Verbindung schliessen und aufraeumen...
	mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
		
    exit(EXIT_SUCCESS);
}
