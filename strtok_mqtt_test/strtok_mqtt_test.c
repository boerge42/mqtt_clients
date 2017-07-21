/*
* strtok_mqtt_test
* ================
* Uwe Berger; 2017
* 
* Test von get_list_value(..) ;-)...
* 
* Beipiel-String:
* ---------------
*  
* current_ts=10.06.2017, 01:32:46|current_temp_out=14.3|current_temp_in=24.3|
* current_hum_in=62.9|current_hum_out=89.0|current_press_rel=1016.1|
* current_press_ris=1|city=Brandenburg|longitude=12.56|latitude=52.41|
* forecast_date=Sat, 10.06.2017, 01:07|sunrise=04:48|sunset=21:31|
* forecast0_day=Sat|forecast0_date=10.06.2017|forecast0_temp_low=12|
* forecast0_temp_high=22|forecast0_text=Scattered Showers|forecast0_code=39|
* forecast1_day=Sun|forecast1_date=11.06.2017|forecast1_temp_low=14|
* forecast1_temp_high=27|forecast1_text=Partly Cloudy|forecast1_code=30|
* forecast2_day=Mon|forecast2_date=12.06.2017|forecast2_temp_low=16|
* forecast2_temp_high=23|forecast2_text=Partly Cloudy|forecast2_code=30|
* forecast3_day=Tue|forecast3_date=13.06.2017|forecast3_temp_low=13|
* forecast3_temp_high=21|forecast3_text=Mostly Cloudy|forecast3_code=28|
* forecast4_day=Wed|forecast4_date=14.06.2017|forecast4_temp_low=12|
* forecast4_temp_high=22|forecast4_text=Partly Cloudy|forecast4_code=30|
* forecast5_day=Thu|forecast5_date=15.06.2017|forecast5_temp_low=13|
* forecast5_temp_high=21|forecast5_text=Partly Cloudy|forecast5_code=30|
* forecast6_day=Fri|forecast6_date=16.06.2017|forecast6_temp_low=13|
* forecast6_temp_high=21|forecast6_text=Mostly Cloudy|forecast6_code=28|
* 
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
#include <mosquitto.h>

// MQTT-Defaults
#define MQTT_HOST 		"dockstar"
#define MQTT_PORT 		1883
#define MQTT_KEEPALIVE 	120

struct mosquitto *mosq	= NULL;
unsigned char debug		= 0; 
unsigned char verbose	= 0; 


// ************************************************
int get_list_value(char *s, char *k, char *v)
{
	char buf[2048];
	char key[255];
	int key_len = 0;
	char *ptr;

	// Ursprungsstring wird durch strtok modifiziert --> Kopie!
	strcpy(buf, s);
	// Defaultvalue initialisieren
	strcpy(v, "-");
	// Liste insgesamt analysieren
	ptr = strtok(buf, "|");
	while(ptr != NULL) {
		// Key ermitteln
		key_len = strchr(ptr, '=') - ptr;
		memcpy(key, ptr, key_len);
		key[key_len] = '\0';
		// ...gesuchter Key?
		if (!strcmp(k, key)) {
			// ...dann den Wert ermitteln/kopieren/fertig!
			strcpy(v, ptr + key_len + 1);
			return 0;	
		}
		// naechstes Listenelement
		ptr = strtok(NULL, "|");
	}
	// Key nicht gefunden...	
	return 1;	
}



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
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	// im Debug-Mode Mosquitto-Log-Message ausgeben
	if (debug) printf("%s\n", str);
}

// ************************************************
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	char v[255];
	
	if(message->payloadlen){
		if (verbose) {
			printf("%s %s\n", message->topic, (char *)message->payload);
		} else {
			printf("%s\n", (char *)message->payload);
			get_list_value((char *)message->payload, "current_ts", v);
			printf("==>> %s\n", v);
			get_list_value((char *)message->payload, "current_hum_in", v);
			printf("==>> %s\n", v);
			get_list_value((char *)message->payload, "uwe", v);
			printf("==>> %s\n", v);
			get_list_value((char *)message->payload, "sunrise", v);
			printf("==>> %s\n", v);
			get_list_value((char *)message->payload, "forecast3_text", v);
			printf("==>> %s\n", v);
		} 
	}else{
		printf("%s (null)\n", message->topic);
	}
}

// ************************************************
// ************************************************
// ************************************************
int main(int argc, char **argv)
{
	char mqtt_host[50]	= MQTT_HOST;
	int  mqtt_port    	= MQTT_PORT;
    char mqtt_topic[255]= "#";
	int c;
	int major, minor, revision;
	
	// Aufrufparameter auslesen/verarbeiten
	while ((c=getopt(argc, argv, "t:h:p:?dv")) != -1) {
		switch (c) {
			case 'h':
				if (strlen(optarg) >= sizeof(mqtt_host)) {
					puts("hostname too long!");
					exit(EXIT_FAILURE);
				} else {
					strcpy(mqtt_host, optarg);
				}
				break;
			case 'p':
				mqtt_port = atoi(optarg);
				break;
			case 't':
				if (strlen(optarg) >= sizeof(mqtt_topic)) {
					puts("topic too long!");
					exit(EXIT_FAILURE);
				} else {
					strcpy(mqtt_topic, optarg);
					puts(mqtt_topic);
				}
				break;
			case 'v':
				verbose=1;
				break;
			case 'd':
				debug=1;
				break;
			case '?':
				puts("mqtt_subscribe [-h <mqtt-host>] [-p <mqtt-port>] [-d] [-t <topic>]");
				puts("Simple mqtt-subscriber; Uwe Berger, 2017");
				exit(0);
				break;
		}
	}
	
	// Init Mosquitto-Lib...
    mosquitto_lib_init();

	// im Debug-Mode Version von libmosquitto ausgeben
	if (debug) {
		mosquitto_lib_version(&major, &minor, &revision);
		printf("libmosquitto-Version: %i.%i.%i\n", major, minor, revision);
	}
	
    // einen Mosquitto-Client erzeugen
    mosq = mosquitto_new(NULL, true, NULL);
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

	// Callbacks definieren
    mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);   

	// mit MQTT-Broker verbinden
    mosquitto_error_handling(mosquitto_connect(mosq, mqtt_host, mqtt_port, MQTT_KEEPALIVE));

	// Topic abonnieren
	mosquitto_error_handling(mosquitto_subscribe(mosq, NULL, mqtt_topic, 0));

	// mqtt-Schleife
   	mosquitto_error_handling(mosquitto_loop_forever(mosq, 100000, 1));

    // Verbindung schliessen und aufraeumen...
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
		
    exit(EXIT_SUCCESS);
}
