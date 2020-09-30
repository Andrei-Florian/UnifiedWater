/*  
	You need to go into file from library and change this line from:
	#define MQTT_MAX_PACKET_SIZE 128
	to:
	#define MQTT_MAX_PACKET_SIZE 2048
*/

#define ARDUINO_MKR; // nessesary for using Logo Library on MKR devices
#include "Universum_Logo.h"

#include <ArduinoLowPower.h>
#include "./global.h"

void setup() 
{
	Serial.begin(9600);
	delay(6000);

	logoStart("WaterAid");

	global.set();
}

void loop() 
{
	//device wakes up
	neo.blink(); // blink the led to identify that device is waking up (debugging purposes)
	Serial.begin(9600);
	logoStart("WaterAid");

	iot.connect(); 			// keep the connection to the backend alive
	delay(1);

	for(int i = 0; i < 10; i++)
	{
		mqtt_client->loop(); 	// background tasks
		Serial.println("Checking for Messages from backend: " + String(i));
		delay(100);
	}

	global.getData();		// collect data from sensor
	global.printData();		// print the data to the serial monitor

	iot.sendTelemetry();	// send the data to the backend

	for(int i = 0; i < 10; i++)
	{
		mqtt_client->loop(); 	// background tasks
		Serial.println("Checking for Messages from backend: " + String(i));
		delay(100);
	}

	iot.sendProperty();		// send all properties to the backend

	// print variables for debugging
	Serial.println(telemetrySendInterval);
	Serial.println(sampleValue);
	Serial.println("");

	// go to sleep 
	uint32_t sleepTime = telemetrySendInterval;
	LowPower.deepSleep(sleepTime);	// sleep for the defined period
}