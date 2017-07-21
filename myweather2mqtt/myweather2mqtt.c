/*
*	  myweather2mqtt
*	 ================
*	 Uwe Berger; 2017
*
*
* Aufruf:
* -------
* myweather2mqtt [-h <mqtt-host>] [-p <mqtt-port>] 
*                [-w <weather_filename>] [-f <forecast_filename>]
* 
* MQTT-Topics:
* ------------
* myweather/timestamp
* myweather/temperature_out
* myweather/temperature_in
* myweather/humidity_in
* myweather/humidity_out
* myweather/pressure_rel
* myweather/lua_list
* 
* weatherforecast/city
* weatherforecast/longitude
* weatherforecast/latitude
* weatherforecast/current_date
* weatherforecast/sunrise
* weatherforecast/sunset
* weatherforecast/forecast0_day
* weatherforecast/forecast0_date
* weatherforecast/forecast0_temp_low
* weatherforecast/forecast0_temp_high
* weatherforecast/forecast0_text
* weatherforecast/forecast0_code
* ... 
* weatherforecast/lua_list
* 
* weatherall/list
* 
* ---------
* Have fun!
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/limits.h>
#include <mosquitto.h>
#include <libconfig.h>

// MQTT-Defaults
#define MQTT_HOST 		"localhost"
#define MQTT_PORT 		1883
#define MQTT_RETAIN		true
#define MQTT_QOS		0
#define MQTT_KEEPALIVE 	120

#define WEATHER_FNAME   "weather_current.conf"
#define FORECAST_FNAME  "weather_forecast.conf"
#define WEATHER_TOPIC   "myweather"
#define FORECAST_TOPIC  "weatherforecast"
#define WEATHERALL_TOPIC "weatherall"
#define FORECAST_DAYS   7

struct mosquitto *mosq = NULL;
unsigned char debug = 0; 

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
}

// ************************************************
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	// im Debug-Mode Mosquitto-Log-Message ausgeben
	if (debug) printf("%s\n", str);
}

// ************************************************
int publish2mqtt(char *topic, char *sub_topic, const char *message, int qos, bool retain)
{
	char topic_all[1024];

	// Topic zusammenbauen und Message versenden
	sprintf(topic_all, "%s/%s", topic, sub_topic);
    mosquitto_error_handling( mosquitto_publish(mosq, NULL, topic_all, strlen(message), message, qos, retain));

	// Message rausschicken, scheint aber auch ohne diesen Loop zu funktionieren...
	mosquitto_error_handling(mosquitto_loop(mosq, 20000, 1));

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

	char weather_fname[PATH_MAX] = WEATHER_FNAME;
	char forecast_fname[PATH_MAX] = FORECAST_FNAME;

    const char *s;
    int rising;
    char buf[255];
    char lua[2048] = "";
    char list[2048] = "";
    char hostname[255] 	= "";
	int c;
	int major, minor, revision;
	config_t cfg;
	int i;

	// Aufrufparameter auslesen/verarbeiten
	while ((c=getopt(argc, argv, "h:p:?df:w:")) != -1) {
		switch (c) {
			case 'h':
				if (strlen(optarg) >= sizeof mqtt_host) {
					puts("hostname too long!");
					exit(EXIT_FAILURE);
				} else {
					strncpy(mqtt_host, optarg, sizeof(mqtt_host));
				}
				break;
			case 'p':
				mqtt_port = atoi(optarg);
				break;
			case 'd':
				debug=1;
				break;
			case 'w':
				strncpy(weather_fname, optarg, sizeof(weather_fname));
				break;
			case 'f':
				strncpy(forecast_fname, optarg, sizeof(forecast_fname));
				break;
			case '?':
				puts("myweather2mqtt [-h <mqtt-host>] [-p <mqtt-port>]");
				puts("               [-w <weatherfile> [-f <forecastfile>]");
				puts("Send weather-/forecast-infos to a mqtt-broker; Uwe Berger, 2017");
				exit(0);
				break;
		}
	}
	
    // (eigenen) Hostname bestimmen
    gethostname(hostname, sizeof(hostname));

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

    // Wetter/-voraussage auslesen und publizieren
    config_init(&cfg);

	// Wetter...
	if(! config_read_file(&cfg, weather_fname)) {
		config_destroy(&cfg);
		return EXIT_FAILURE;
	}
	sprintf(lua, "%s", "{");
    if (config_lookup_string(&cfg, "timestamp", &s)) {
		publish2mqtt(WEATHER_TOPIC, "timestamp", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "timestamp", s);
		sprintf(list, "%s%s=%s|", list, "current_ts", s);
	}
    if (config_lookup_string(&cfg, "temperature_out", &s)) {
		publish2mqtt(WEATHER_TOPIC, "temperature_out", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "temperature_out", s);
		sprintf(list, "%s%s=%s|", list, "current_temp_out", s);
	}
    if (config_lookup_string(&cfg, "temperature_in", &s)) {
		publish2mqtt(WEATHER_TOPIC, "temperature_in", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "temperature_in", s);
		sprintf(list, "%s%s=%s|", list, "current_temp_in", s);
	}
    if (config_lookup_string(&cfg, "humidity_in", &s)) {
		publish2mqtt(WEATHER_TOPIC, "humidity_in", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "humidity_in", s);
		sprintf(list, "%s%s=%s|", list, "current_hum_in", s);
	}
    if (config_lookup_string(&cfg, "humidity_out", &s)) {
		publish2mqtt(WEATHER_TOPIC, "humidity_out", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "humidity_out", s);
		sprintf(list, "%s%s=%s|", list, "current_hum_out", s);
	}
    if (config_lookup_string(&cfg, "pressure_rel", &s)) {
		publish2mqtt(WEATHER_TOPIC, "pressure_rel", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "pressure_rel", s);
		sprintf(list, "%s%s=%s|", list, "current_press_rel", s);
	}
    if (config_lookup_int(&cfg, "pressure_rising", &rising)) {
		sprintf(buf, "%i", rising);
		publish2mqtt(WEATHER_TOPIC, "pressure_rising", buf, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%i\"", lua, "pressure_rising", rising);
		sprintf(list, "%s%s=%i|", list, "current_press_ris", rising);
	}
	sprintf(lua, "%s%s", lua, "}");
	// .. eine Lua-Liste ;-)
	publish2mqtt(WEATHER_TOPIC, "lua_list", lua, mqtt_qos, mqtt_retain);

	// Forecast...
	if(! config_read_file(&cfg, forecast_fname)) {
		config_destroy(&cfg);
		return EXIT_FAILURE;
	}
	sprintf(lua, "%s", "{");
    if (config_lookup_string(&cfg, "city", &s)) {
		publish2mqtt(FORECAST_TOPIC, "city", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "city", s);
		sprintf(list, "%s%s=%s|", list, "city", s);
	}
    if (config_lookup_string(&cfg, "longitude", &s)) {
		publish2mqtt(FORECAST_TOPIC, "longitude", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "longitude", s);
		sprintf(list, "%s%s=%s|", list, "longitude", s);
	}
    if (config_lookup_string(&cfg, "latitude", &s)) {
		publish2mqtt(FORECAST_TOPIC, "latitude", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "latitude", s);
		sprintf(list, "%s%s=%s|", list, "latitude", s);
	}
    if (config_lookup_string(&cfg, "current_date", &s)) {
		publish2mqtt(FORECAST_TOPIC, "current_date", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "current_date", s);
		sprintf(list, "%s%s=%s|", list, "forecast_date", s);
	}
    if (config_lookup_string(&cfg, "sunrise", &s)) {
		publish2mqtt(FORECAST_TOPIC, "sunrise", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "sunrise", s);
		sprintf(list, "%s%s=%s|", list, "sunrise", s);
	}
    if (config_lookup_string(&cfg, "sunset", &s)) {
		publish2mqtt(FORECAST_TOPIC, "sunset", s, mqtt_qos, mqtt_retain);
		sprintf(lua, "%s%s=\"%s\",", lua, "sunset", s);
		sprintf(list, "%s%s=%s|", list, "sunset", s);
	}

	sprintf(lua, "%s%s", lua, "fc={");	
	for (i = 0; i < FORECAST_DAYS; i++) {
		
		if (i) {
			sprintf(lua, "%s,%s", lua, "{");	
		} else {
			sprintf(lua, "%s%s", lua, "{");				
		}
		
		sprintf(buf, "forecast%i_day", i);
		if (config_lookup_string(&cfg, buf, &s)) {
			sprintf(buf, "%i/day", i);
			publish2mqtt(FORECAST_TOPIC, buf, s, mqtt_qos, mqtt_retain);
			sprintf(lua, "%s%s=\"%s\"", lua, "day", s);
			sprintf(list, "%s%s%i%s=%s|", list, "forecast", i, "_day", s);
		}
		sprintf(buf, "forecast%i_date", i);
		if (config_lookup_string(&cfg, buf, &s)){
			sprintf(buf, "%i/date", i);
			publish2mqtt(FORECAST_TOPIC, buf, s, mqtt_qos, mqtt_retain);
			sprintf(lua, "%s,%s=\"%s\"", lua, "date", s);
			sprintf(list, "%s%s%i%s=%s|", list, "forecast", i, "_date", s);
		}
		sprintf(buf, "forecast%i_temp_low", i);
		if (config_lookup_string(&cfg, buf, &s)){
			sprintf(buf, "%i/temp_low", i);
			publish2mqtt(FORECAST_TOPIC, buf, s, mqtt_qos, mqtt_retain);
			sprintf(lua, "%s,%s=\"%s\"", lua, "temp_low", s);
			sprintf(list, "%s%s%i%s=%s|", list, "forecast", i, "_temp_low", s);
		}
		sprintf(buf, "forecast%i_temp_high", i);
		if (config_lookup_string(&cfg, buf, &s)){
			sprintf(buf, "%i/temp_high", i);
			publish2mqtt(FORECAST_TOPIC, buf, s, mqtt_qos, mqtt_retain);
			sprintf(lua, "%s,%s=\"%s\"", lua, "temp_high", s);
			sprintf(list, "%s%s%i%s=%s|", list, "forecast", i, "_temp_high", s);
		}
		sprintf(buf, "forecast%i_text", i);
		if (config_lookup_string(&cfg, buf, &s)){
			sprintf(buf, "%i/text", i);
			publish2mqtt(FORECAST_TOPIC, buf, s, mqtt_qos, mqtt_retain);
			sprintf(lua, "%s,%s=\"%s\"", lua, "text", s);
			sprintf(list, "%s%s%i%s=%s|", list, "forecast", i, "_text", s);
		}
		sprintf(buf, "forecast%i_code", i);
		if (config_lookup_string(&cfg, buf, &s)){
			sprintf(buf, "%i/code", i);
			publish2mqtt(FORECAST_TOPIC, buf, s, mqtt_qos, mqtt_retain);
			sprintf(lua, "%s,%s=\"%s\"", lua, "code", s);
			sprintf(list, "%s%s%i%s=%s|", list, "forecast", i, "_code", s);
		}
		sprintf(lua, "%s%s", lua, "}");		
	}
	sprintf(lua, "%s%s", lua, "}");
	sprintf(lua, "%s%s", lua, "}");
	
	// ...und senden
	publish2mqtt(FORECAST_TOPIC, "lua_list", lua, mqtt_qos, mqtt_retain);
	publish2mqtt(WEATHERALL_TOPIC, "list", list, mqtt_qos, mqtt_retain);

	// ...hmm, warum werden sonst nicht alle Nachrichten gesendet?
   	usleep(100000);

    // Verbindung schliessen und aufraeumen...
	mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
   	config_destroy(&cfg);

		
    exit(EXIT_SUCCESS);
}
