/*  
	You need to go into file from library and change this line from:
	#define MQTT_MAX_PACKET_SIZE 128
	to:
	#define MQTT_MAX_PACKET_SIZE 2048
*/

#define ARDUINO_MKR; // nessesary for using Logo Library on MKR devices
#include "Universum_Logo.h"
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
	if(deviceMode == ENTERPRISE_MODE) // if in enterprise mode
	{
		// collect data
		if(millis() > (lastSensorReadMillis + dataCollectionInterval))
		{
			global.getData();
			global.printData();

			lastSensorReadMillis = millis();
		}

		// send data
		if(millis() > (lastTelemetryMillis + telemetrySendInterval))
		{
			iot.sendTelemetry();
			lastTelemetryMillis = millis();
		}
	}
	else // if in personal mode
	{
		if(get.button()) // if the button is pressed
		{
			Serial.println("[loop] BUTTON PRESSED");

			global.getData();
			global.printData();

			iot.sendTelemetry();
		}
	}

	// send properties
	if(millis() > (lastPropertyMillis + propertySendInterval) || updatePropertiesNow)
	{
		iot.sendProperty();
		lastPropertyMillis = millis();
		updatePropertiesNow = false;
	}

	iot.connect(); // keep the connection to the backend alive
	mqtt_client->loop(); // background tasks
	delay(1); // run the loop every 100ms

	// // print variables for debugging
	// Serial.println(telemetrySendInterval);
	// Serial.println(dataCollectionInterval);
	// Serial.println(sampleValue);
	// Serial.println(propertySendInterval);
	// Serial.println(deviceMode);
	// Serial.println("");
}