/*
*	 mqtt_subscribe
*	================
*	Uwe Berger; 2017
*
*
* Aufruf:
* -------
* RTFM, ...aehm Quelltext...;-)
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
	if(message->payloadlen){
		if (verbose) {
			printf("%s %s\n", message->topic, (char *)message->payload);
		} else {
			printf("%s\n", (char *)message->payload);
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
