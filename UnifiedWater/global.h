#include "IotCentral.h"

void iotset()
{
    //begin rtc
    rtc.begin();

    delay(100);

    // connect to GSM network
    Serial.println("[IoT] Getting IoT Hub host from Azure IoT DPS");
    servicesStatus[DPS_STATUS] = PENDING;
    
    char hostName[64] = {0};
    
    // compute derived key
    int keyLength = strlen(iotc_enrollmentKey);
    int decodedKeyLength = base64_dec_len(iotc_enrollmentKey, keyLength);
    char decodedKey[decodedKeyLength];
    base64_decode(decodedKey, iotc_enrollmentKey, keyLength);
    Sha256 *sha256 = new Sha256();
    sha256->initHmac((const uint8_t*)decodedKey, (size_t)decodedKeyLength);
    sha256->print(deviceId.c_str());
    char* sign = (char*) sha256->resultHmac();
    memset(iotc_enrollmentKey, 0, sizeof(iotc_enrollmentKey)); 
    base64_encode(iotc_enrollmentKey, sign, HASH_LENGTH);
    delete(sha256);
    neo.show(SETUP_PROGRESS, 12, 13);

    getHubHostName((char*)iotc_scopeId, (char*)deviceId.c_str(), (char*)iotc_enrollmentKey, hostName);
    servicesStatus[DPS_STATUS] = CONNECTED;
    
    iothubHost = hostName;
    Serial.println("");
    Serial.println("[IoT] Hostname: " + String(hostName));

    // create SAS token and user name for connecting to MQTT broker
    mqttUrl = iothubHost + urlEncode(String((char*)F("/devices/") + deviceId).c_str());
    String password = "";
    long expire = rtctime.get() + 864000;
    password = createIotHubSASToken(iotc_enrollmentKey, mqttUrl, expire);
    userName = iothubHost + "/" + deviceId + (char*)F("/?api-version=2018-06-30");
    neo.show(SETUP_PROGRESS, 13, 14);

    // connect to the IoT Hub MQTT broker
    mqtt_client = new PubSubClient(mqttSslClient);
    mqtt_client->setServer(iothubHost.c_str(), 8883);
    mqtt_client->setCallback(callback);
    
    connectMQTT(deviceId, userName, password);
    neo.show(SETUP_PROGRESS, 14, 15);
    
    // request full digital twin update
    String topic = (String)IOT_TWIN_REQUEST_TWIN_TOPIC;
    char buff[20];
    topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
    twinRequestId = requestId;
    requestId++;
    servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
    mqtt_client->publish(topic.c_str(), "");
    servicesStatus[MQTT_STATUS] = CONNECTED;  ;
    
    // initialize timers
    lastTelemetryMillis = millis();
    lastPropertyMillis = millis();
    lastSensorReadMillis = millis();
}

struct Global
{
	private:
		// private functions
		void getGeo()
		{
			loc.get(); // lock onto the satellite and get the geolocation values

			// set the global variables
			location.latitude = loc.latitude;
			location.longitude = loc.longitude;
			location.altitude = loc.altitude;
            location.accuracy = loc.gpsacc;
		}

	public:
		// global variables
        struct Location
        {
            double latitude;
            double longitude;
            double altitude;
            long accuracy;
        };

        struct Status
        {
            unsigned long localTime;
            double batteryLevel;
        };

        struct Data
        {
            double waterPh;
            double waterTurbidity;
            double waterTemp;
            double atmoTemp;
            double atmoHumidity;
        };

        Location location;
        Status status;
        Data data;

        // record the millis() when actions were taken
        long lastProperty;
        long lastTelemetry;
        long lastSensorRead;
        
		// global functions
		void set()
		{
            Serial.println("[setup] Procedure Initiated");
			neo.init(); // set the onboard LED as an output and enable control
            
            neo.show(WAKEUP, 0, 0);
            delay(500);
            neo.show(SETUP_PROGRESS, 0, 2);

            get.init(); // initialise the sensors
            neo.show(SETUP_PROGRESS, 2, 4);

			gsm.init(); // initialise the GSM service
            neo.show(SETUP_PROGRESS, 4, 6);
			
			rtctime.set(); // get the time from the time server and synch the onboard RTC to it
            neo.show(SETUP_PROGRESS, 6, 9);

			loc.set(); // lock onto the location from the GNSS service
            neo.show(SETUP_PROGRESS, 9, 12);

            iotset(); // connect to IoT Central
            mqtt_client->setKeepAlive(keepConnectionAlive);
            neo.show(SETUP_PROGRESS, 15, 16);

            Serial.println("[setup] complete");
			Serial.println("");
			Serial.println("");
            neo.hide(true);
		}

		void getData()
		{
            Serial.println("[loop] Collecting Data from sensors");

            // geolocation is not collected in enterprise mode because device is stationary (set through cloud property)
            if(deviceMode == PERSONAL_MODE)
            {
			    this->getGeo(); // get the geolocation
            }

            neo.show(COLLECT_PROGRESS, 0, 2);
            
			status.localTime = rtctime.get(); // get the time
            neo.show(COLLECT_PROGRESS, 2, 4);

            status.batteryLevel = battery.get(); // get the voltage of the battery
            neo.show(COLLECT_PROGRESS, 4, 6);

            // collect sensor Data
            data.waterPh = get.pH();
            neo.show(COLLECT_PROGRESS, 6, 10);

            data.waterTurbidity = get.turbidity();
            neo.show(COLLECT_PROGRESS, 10, 13);

            data.waterTemp = get.temperature();
            neo.show(COLLECT_PROGRESS, 13, 14);

            data.atmoTemp = get.thSensor.temperature();
            neo.show(COLLECT_PROGRESS, 14, 15);

            data.atmoHumidity = get.thSensor.humidity();
            neo.show(COLLECT_PROGRESS, 15, 16);

            Serial.println("\n");
            neo.hide(true);
		}

		void printData()
		{
			Serial.println("[loop] Data Collected");

            if(deviceMode == PERSONAL_MODE)
            {
                Serial.println("[location]  Latitude          " + String(location.latitude, 6));
                Serial.println("[location]  Longitude         " + String(location.longitude, 6));
                Serial.println("[location]  Altitude          " + String(location.altitude, 6));
                Serial.println("[location]  Accuracy          " + String(location.accuracy));

                Serial.println("");
            }

			Serial.println("[status]    Time              " + String(status.localTime));
			Serial.println("[status]    Battery           " + String(status.batteryLevel));
            
            Serial.println("");

			Serial.println("[data]      waterPh           " + String(data.waterPh));
			Serial.println("[data]      waterTurbidity    " + String(data.waterTurbidity));
			Serial.println("[data]      waterTemp         " + String(data.waterTemp));
			Serial.println("[data]      atmoTemp          " + String(data.atmoTemp));
			Serial.println("[data]      atmoHumidity      " + String(data.atmoHumidity));

            Serial.println("\n");
		} 
};

Global global;

struct IotCentral
{
    private:
        String returnDeviceMode()
        {
            if(deviceMode)
            {
                return "true";
            }

            return "false";
        }
    public:
        void connect()
        {
            if(!mqtt_client->connected())
            {
                Serial.println("[IoT] MQTT connection lost");
                // try to reconnect
                gsm.init();
                servicesStatus[DPS_STATUS] = CONNECTED; // we won't reprovision so provide feedback that last DPS call succeeded.
                
                // since disconnection might be because SAS Token has expired or has been revoked (e.g device had been blocked), regenerate a new one
                long expire = rtctime.get() + 864000;
                password = createIotHubSASToken(iotc_enrollmentKey, mqttUrl, expire);
                connectMQTT(deviceId, userName, password);
            }

            // give the MQTT handler time to do it's thing
            mqtt_client->loop();
        }

        void sendTelemetry()
        {
            Serial.println("");
            Serial.println("[loop] Sending telemetry to IoT Central");
            neo.show(SEND_PROGRESS, 0, 4);

            String topic = (String)IOT_EVENT_TOPIC;
            topic.replace(F("{device_id}"), deviceId);

            String payload;
            
            if(deviceMode == PERSONAL_MODE)
            {
                payload = personalTelemetryPayload;

                payload.replace(F("{LOCATION}"), "\"location\": { \"lat\": {lat}, \"lon\": {lon}, \"alt\": {alt}}");
                payload.replace(F("{lat}"), String(global.location.latitude, 7));
                payload.replace(F("{lon}"), String(global.location.longitude, 7));
                payload.replace(F("{alt}"), String(global.location.altitude, 7));
            }
            else
            {
                payload = enterpriseTelemetryPayload;
            }

            neo.show(SEND_PROGRESS, 4, 8);

            payload.replace(F("{waterPh}"), String(global.data.waterPh, 2));
            payload.replace(F("{waterTurbidity}"), String(global.data.waterTurbidity, 2));
            payload.replace(F("{waterTemp}"), String(global.data.waterTemp, 2));
            payload.replace(F("{atmoTemp}"), String(global.data.atmoTemp, 2));
            payload.replace(F("{atmoHumidity}"), String(global.data.atmoHumidity, 2));
            neo.show(SEND_PROGRESS, 8, 12);
            
            Serial.println("[IoT] " + String(payload.c_str()));
            servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
            mqtt_client->publish(topic.c_str(), payload.c_str());
            servicesStatus[MQTT_STATUS] = CONNECTED;  ;

            Serial.println("[IoT] Telemetry is sent");
            neo.show(SEND_PROGRESS, 12, 16);
            Serial.println("");
            neo.hide(true);
        }

        void sendProperty()
        {
            Serial.println("");
            Serial.println("[loop] Sending digital twin properties");
            neo.show(SEND_PROGRESS, 0, 4);

            String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
            char buff[60];
            topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
            neo.show(SEND_PROGRESS, 4, 8);

            String payload = propertyPayload;
            if(deviceMode == ENTERPRISE_MODE)
            {
                payload.replace(F("\"locationAccuracy\"           : {accuracy},"), "");
            }
            else
            {
                payload.replace(F("{accuracy}"), String(global.location.accuracy, 7));
            }
            
            payload.replace(F("{bat}"), String(global.status.batteryLevel, 2));

            payload.replace(F("{telemetrySendInterval}"), String(telemetrySendInterval));
            payload.replace(F("{sampleValue}"), String(sampleValue));
            payload.replace(F("{dataCollectionInterval}"), String(dataCollectionInterval));
            payload.replace(F("{propertySendInterval}"), String(propertySendInterval));
            payload.replace(F("{deviceMode}"), returnDeviceMode());
            neo.show(SEND_PROGRESS, 8, 12);

            servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
            mqtt_client->publish(topic.c_str(), payload.c_str());
            servicesStatus[MQTT_STATUS] = CONNECTED;  ;
            Serial.println("[IoT] " + String(payload.c_str()));
            Serial.println("[IoT] Digital Twin properties are sent");
            Serial.println("");
            
            neo.show(SEND_PROGRESS, 12, 16);
            neo.hide(true);
            requestId++;
        }
};

IotCentral iot;