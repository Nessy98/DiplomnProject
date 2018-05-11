#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "MQTTClient.h"
#include "MQTTClientPersistence.h"

#define BROKER     "127.0.0.1"
#define CLIENTID    "raspi"
#define PAYLOAD     "001"
#define QOS         2	
#define TIMEOUT     10000L

// Message headers
#define SERVER		"/system_name/server"
#define CONFIG		"/config"
#define DATA		"/data"
#define ACKNOWLEDGE "/config_ack"
#define SYSTEM		"/system_name/"
#define DISCOVER	"/system_name/discover"
#define IDLEMESSAGE "op_Mode: IDLE; sleep_time: 50000"

static MQTTClient client;

volatile MQTTClient_deliveryToken deliveredtoken;

void delivered(void *context, MQTTClient_deliveryToken dt){
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken =  dt;
}

void subscribeForSensorUnit(char* discover){
	
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;
	
	printf("%s\n", discover);
	
	const size_t systemLen = strlen(SYSTEM);
	const size_t discoverLen = strlen(discover);
	
	const size_t dataLen = strlen(DATA);
	const size_t ackLen = strlen(ACKNOWLEDGE);
	const size_t configLen = strlen(CONFIG);
	
	char* sensorDataTopic = malloc(systemLen + discoverLen + dataLen  + 1);
	
	memcpy(sensorDataTopic, SYSTEM, systemLen);
	memcpy(sensorDataTopic + systemLen, discover, discoverLen);
	memcpy(sensorDataTopic + systemLen + discoverLen, DATA, dataLen + 1);
	
	char* configAckTopic = malloc(systemLen + discoverLen + ackLen + 1);
	
	memcpy(configAckTopic, SYSTEM, systemLen);
	memcpy(configAckTopic + systemLen, discover, discoverLen);
	memcpy(configAckTopic + systemLen + discoverLen, ACKNOWLEDGE, ackLen + 1);
	
	int rc = MQTTClient_subscribe(&client, configAckTopic, QOS);
	printf("Status: %d\n", rc);
	if(rc == 0){
		printf("Subscribe for topic %s successful\n", configAckTopic);
	}

	free(sensorDataTopic);
	free(configAckTopic);
	
	char* configTopic = malloc(systemLen + discoverLen + configLen);
	memcpy(configTopic, SYSTEM, systemLen);
	memcpy(configTopic + systemLen, discover, discoverLen);
	memcpy(configTopic + systemLen + discoverLen, CONFIG, configLen + 1);
	
	pubmsg.payload = IDLEMESSAGE;
    pubmsg.payloadlen = strlen(pubmsg.payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
	
	MQTTClient_publishMessage(client, configTopic, &pubmsg, &token);
    printf("Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            (int)(TIMEOUT/500), pubmsg.payload, configTopic, CLIENTID);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
	
	printf("Message with delivery token %d delivered\n", token);
	
	free(configTopic);
}

void loadDataToDB(char* dataMessage){
	// load sensor data to the db
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   int len = 0;
   char* unitName = malloc(strlen(argv[1]) + 1);
   char* unitTemp;
   


   for(i = 0; i < argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	  if(strstr(azColName[i], "name")  != NULL) {
		printf("Col = name\n");
		//strcpy(message, argv[i]);
	  }
   }
   //printf("Message : %s\n", message);
   return 0;
}

void loadDataToServer(){
	sqlite3 *db;
	sqlite3_stmt *stmt;
	char *errMsg = 0;
	
	int rc = sqlite3_open("automation.db", &db);
	
	if(rc){
		printf("Failed to open\n");
	}else{
		printf("Performing query...\n");
		sqlite3_prepare_v2(db, "select * from SensorUnit", -1, &stmt, NULL);
		
		printf("Got results:\n");
		while (sqlite3_step(stmt) != SQLITE_DONE) {
			int i;
			int num_cols = sqlite3_column_count(stmt);
			
			for (i = 0; i < num_cols; i++)
			{
				switch (sqlite3_column_type(stmt, i))
				{
				case (SQLITE3_TEXT):
					printf("%s, ", sqlite3_column_text(stmt, i));
					break;
				case (SQLITE_INTEGER):
					printf("%d, ", sqlite3_column_int(stmt, i));
					break;
				case (SQLITE_FLOAT):
					printf("%g, ", sqlite3_column_double(stmt, i));
					break;
				default:
					break;
				}
			}
			printf("\n");
		}
	}
	sqlite3_finalize(stmt);
	
	sqlite3_close(db);
}

int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message){
    int count;
    char* payloadptr;

    printf("Message arrived\n");
    printf("    topic: %s\n", topicName);
    printf("	message: ");
    payloadptr = message->payload;
	
    for(count = 0; count < message->payloadlen; count ++){
        putchar(*payloadptr ++);
    }
	
	putchar('\n');
	
	if(strstr(topicName, DISCOVER) != NULL){
		//check if the arrieved discover already exist in the db
		// if not wirte it to the db and subscribe
		subscribeForSensorUnit(message->payload);
	}else if(strstr(topicName, DATA) != NULL){
		loadDataToDB(message->payload);
	}else if(strstr(topicName, ACKNOWLEDGE) != NULL){
		
	}else if(strstr(topicName, SERVER) != NULL){
		loadDataToServer();
	}
	
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
	
	
    return 1;
}

void connlost(void *context, char *cause){
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}


int main(int argc, char* argv[]){
	
	int rc;
	char* username = "mosquitto"; 
	char* password = "password";

	
	MQTTClient_deliveryToken token;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = username;
	conn_opts.password = password;

	MQTTClient_create(&client, BROKER, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
	
	MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);

    if((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS){
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }
	
	printf("Successful conntection with the Brocker.\n");
	
	
	rc = MQTTClient_subscribe(client, DISCOVER, QOS);
	if(rc == 0){
		printf("Subscribed for topic %s successful\n", DISCOVER);
	}
	
	rc = MQTTClient_subscribe(client, SERVER, QOS);
	if(rc == 0){
		printf("Subscribed for topic %s successful\n", SERVER);
	}
    
	while(1){}
	
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
	 
	return rc;
}