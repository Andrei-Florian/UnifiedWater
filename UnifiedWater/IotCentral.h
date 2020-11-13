// external dependencies
#include <time.h>
#include <SPI.h>
#include <MKRGSM.h>
#include <RTCZero.h>
#include <Arduino_MKRENV.h> // delete
#include <PubSubClient.h>
#include <Reset.h>
#include <ArduinoHttpClient.h>

// links to files
#include "./sha256.h"
#include "./base64.h"
#include "./parson.h"
#include "./utils.h"
#include "./InternalStorage.h" // copied from Wifi101OTA
#include "./UrlParser.h"

// pointers
int getHubHostName(char *scopeId, char* deviceId, char* key, char *hostName);

// Globals
String iothubHost;
String mqttUrl;
String userName;
String password;

GSMSSLClient mqttSslClient;
PubSubClient *mqtt_client = NULL;

bool mqttConnected = false;

long lastTelemetryMillis = 0;
long lastPropertyMillis = 0;
long lastSensorReadMillis = 0;

double tempValue = 0.0;
double humidityValue = 0.0;
double batteryValue = 0.0;
bool panicButtonPressed = false;

typedef struct
{
    int signalQuality;
    bool isSimDetected;
    bool isRoaming;
    char imei[20];
    char iccid[20];
    long up_bytes;
    long dn_bytes;
} GsmInfo;

GsmInfo gsmInfo { 0, false, false, "", "", 0, 0};

enum connection_status { DISCONNECTED, PENDING, CONNECTED, TRANSMITTING};

#define GPRS_STATUS 0
#define DPS_STATUS 1
#define MQTT_STATUS 2
connection_status servicesStatus[3] = { DISCONNECTED, DISCONNECTED, DISCONNECTED};

// MQTT publish topics
static const char PROGMEM IOT_EVENT_TOPIC[] = "devices/{device_id}/messages/events/";
static const char PROGMEM IOT_TWIN_REPORTED_PROPERTY[] = "$iothub/twin/PATCH/properties/reported/?$rid={request_id}";
static const char PROGMEM IOT_TWIN_REQUEST_TWIN_TOPIC[] = "$iothub/twin/GET/?$rid={request_id}";
static const char PROGMEM IOT_DIRECT_METHOD_RESPONSE_TOPIC[] = "$iothub/methods/res/{status}/?$rid={request_id}";

// MQTT subscribe topics
static const char PROGMEM IOT_TWIN_RESULT_TOPIC[] = "$iothub/twin/res/#";
static const char PROGMEM IOT_TWIN_DESIRED_PATCH_TOPIC[] = "$iothub/twin/PATCH/properties/desired/#";
static const char PROGMEM IOT_C2D_TOPIC[] = "devices/{device_id}/messages/devicebound/#";
static const char PROGMEM IOT_DIRECT_MESSAGE_TOPIC[] = "$iothub/methods/POST/#";

int requestId = 0;
int twinRequestId = -1;

#include "trace.h"
#include "iotc_dps.h"
#include "pelion_certs.h"

void acknowledgeSetting(const char* propertyKey, const char* propertyValue, int version, bool success) 
{
    // for IoT Central need to return acknowledgement
    const static char* PROGMEM responseTemplate = "{\"%s\":{\"value\":%s,\"statusCode\":%d,\"status\":\"%s\",\"desiredVersion\":%d}}";
    char payload[1024];

    if(!success) // check if the message being sent is a success message
    {
        sprintf(payload, responseTemplate, propertyKey, propertyValue, 401, F("unauthorized"), version);
    }
    else
    {
        sprintf(payload, responseTemplate, propertyKey, propertyValue, 200, F("completed"), version);
    }

    Serial.println("[IoT] Sending acknowledgement: " + String(payload));
    String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
    char buff[20];
    topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
    servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
    mqtt_client->publish(topic.c_str(), payload);
    servicesStatus[MQTT_STATUS] = CONNECTED;  ;
    requestId++;
}

void handleDirectMethod(String topicStr, String payloadStr) 
{
    String msgId = topicStr.substring(topicStr.indexOf("$RID=") + 5);
    String methodName = topicStr.substring(topicStr.indexOf(F("$IOTHUB/METHODS/POST/")) + 21, topicStr.indexOf("/?$"));
    Serial.println("[IoT] Direct method call");
    Serial.println("      Method Name:  " + String(methodName.c_str()));
    Serial.println("      Payload Name: " + String(payloadStr.c_str()));

    String status = String(F("OK"));
    if (strcmp(methodName.c_str(), "DOWNLOAD_FIRMWARE") == 0) {
        JSON_Value *root_value = json_parse_string(payloadStr.c_str());
        JSON_Object *root_obj = json_value_get_object(root_value);
        const char *package_uri = json_object_get_string(root_obj, "package_uri");
        Serial.println("[IoT] " + String(package_uri));
        UrlParser parser;
        UrlParserResult parsed_url;
        if (parser.parse(package_uri, parsed_url)) {
            // download package
            if (parsed_url.port == 0) {
                parsed_url.port = 80; // TODO URL might be https protocol so default port would be different...
            } 
            HttpClient httpClient = HttpClient(client, parsed_url.host.c_str(), parsed_url.port);
            Serial.print("Connecting to http://");
            Serial.print(parsed_url.host);
            Serial.print(":");
            Serial.print(parsed_url.port);
            Serial.println("[IoT] " + String(parsed_url.path));
            

            int err = httpClient.get(parsed_url.path);
            int fileLength = 0 ;
            if (err == HTTP_SUCCESS) {
                err = httpClient.responseStatusCode();
                if (err == 200) { // HTTP 200 
                    unsigned long timeoutStart = millis();
                    if(httpClient.skipResponseHeaders() == HTTP_SUCCESS) {
                        if(InternalStorage.open()) {
                            char c;
                            // Whilst we haven't timed out & haven't reached the end of the body
                            while ((httpClient.connected() || httpClient.available()) &&
                                (!httpClient.endOfBodyReached()) &&
                                ((millis() - timeoutStart) < 30 * 1000)) { // timeout 30s
                                if (httpClient.available()) {
                                    c = (char)httpClient.read();
                                    InternalStorage.write(c);
                                    fileLength++ ;
                                    // We read something, reset the timeout counter
                                    timeoutStart = millis();
                                }
                            }
                        } else {
                            Serial.println("[IoT] InternalStorage::open() failed");
                        }
                    }
                    httpClient.stop();
                    InternalStorage.close();
                    Serial.println("[IoT] Successfully downloaded FW - size: " + String(fileLength));
                } else {
                    Serial.println("[IoT] Cannot get HTTP response");
                    Serial.println("[IoT]  ERROR - " + String(err));
                }
            } else {
                Serial.println("[IoT] Connection failed");
            }
        } else {
            Serial.println("[IoT] Can't parse URI");
        }
        // acknowledge receipt of the command
        String response_topic = (String)IOT_DIRECT_METHOD_RESPONSE_TOPIC;
        char buff[20];
        response_topic.replace(F("{request_id}"), msgId);
        response_topic.replace(F("{status}"), F("200")); //OK
        servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
        mqtt_client->publish(response_topic.c_str(), "");
        servicesStatus[MQTT_STATUS] = CONNECTED;  ;
    } else if (strcmp(methodName.c_str(), "APPLY_FIRMWARE") == 0) {
        // acknowledge receipt of the command
        String response_topic = (String)IOT_DIRECT_METHOD_RESPONSE_TOPIC;
        char buff[20];
        response_topic.replace(F("{request_id}"), msgId);
        response_topic.replace(F("{status}"), F("200"));  //OK
        servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
        mqtt_client->publish(response_topic.c_str(), "");
        servicesStatus[MQTT_STATUS] = CONNECTED;  ;
        Serial.println("[IoT] /!\\ DEVICE WILL NOW REBOOT WITH NEW FIRMWARE /!\\");
        InternalStorage.apply();
        while (true);
    }
}

void handleCloud2DeviceMessage(String topicStr, String payloadStr) 
{
    Serial.println("[IoT] Cloud to device call: Payload: " + String(payloadStr.c_str()));
}

void handleTwinPropertyChange(String topicStr, String payloadStr) 
{
    Serial.println("");

    Serial.println("[loop] Processing Received Digital twin change");
    Serial.println("[IoT] handleTwinPropertyChange - " + String(payloadStr.c_str()));
    neo.show(SEND_PROGRESS, 0, 2);

    // read the property values sent using JSON parser
    JSON_Value *root_value = json_parse_string(payloadStr.c_str());
    JSON_Object *root_obj = json_value_get_object(root_value);
    neo.show(SEND_PROGRESS, 2, 4);

    // variables to store values
    const char* propertyKeyRAW = json_object_get_name(root_obj, 0);
    double propertyValueNum = 0;
    double version = 0;

    // store the values in variables created
    JSON_Object* valObj = json_object_get_object(root_obj, propertyKeyRAW);
    propertyValueNum = json_object_get_number(root_obj, propertyKeyRAW);
    version = json_object_get_number(root_obj, "$version");
    String propertyKey = String(propertyKeyRAW);
    neo.show(SEND_PROGRESS, 4, 6);

    // print these values
    Serial.println("    propertyValueNum : " + String(propertyValueNum));
    Serial.println("    propertyKey      : " + String(propertyKey));
    Serial.println("    version          : " + String(version));

    char propertyValueStr[20];
    bool error = false;

    if(propertyValueNum > 2000000000) // check if the number is too big
    {
        Serial.println("[IoT] Error - The number provided through the portal is too big to store");
        error = true;
    }

    neo.show(SEND_PROGRESS, 6, 8);

    if(propertyKey == "dataCollectionInterval")
    {
        if(error)
        {
            propertyValueNum = 20000;
        }

        dataCollectionInterval = propertyValueNum;
    }
    else if(propertyKey == "sampleValue")
    {
        if(error)
        {
            propertyValueNum = 20;
        }

        sampleValue = propertyValueNum;
    }
    else if(propertyKey == "telemetrySendInterval")
    {
        if(error)
        {
            propertyValueNum = 50000;
        }

        telemetrySendInterval = propertyValueNum;
    }
    else if(propertyKey == "propertySendInterval")
    {
        if(error)
        {
            propertyValueNum = 5000;
        }

        propertySendInterval = propertyValueNum;
    }
    else if(propertyKey == "deviceMode")
    {
        if(payloadStr.indexOf("true") > 0)
        {
            Serial.println("    set to true");

            lastSensorReadMillis = millis();
            lastTelemetryMillis = millis();
            neo.hide(true);

            deviceMode = true;
        }
        else
        {
            Serial.println("    set to false");
            deviceMode = false;

            neo.hide(true);
        }
    }
    else
    {
        Serial.println("[IoT] Error - the property changed is not found");
    }

    neo.show(SEND_PROGRESS, 8, 12);

    itoa(propertyValueNum, propertyValueStr, 10);
    Serial.println("\n[IoT] Sending back value: " + String(propertyValueStr));
    if(error)
    {
        acknowledgeSetting(propertyKeyRAW, propertyValueStr, version, false);
    }
    else
    {
        acknowledgeSetting(propertyKeyRAW, propertyValueStr, version, true);
    }

    json_value_free(root_value);
    updatePropertiesNow = true;
    neo.show(SEND_PROGRESS, 12, 16);

    Serial.println("");
    neo.hide(true);
    delay(1000);
    neo.show(TWIN_RECEIVE, 0, 0);
}

// http://atmel.force.com/support/articles/en_US/FAQ/Reading-unique-serial-number-on-SAM-D20-SAM-D21-SAM-R21-devices
String chipId() 
{
  volatile uint32_t val1, val2, val3, val4;
  volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
  val1 = *ptr1;
  volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
  val2 = *ptr;
  ptr++;
  val3 = *ptr;
  ptr++;
  val4 = *ptr;

  char buf[33];
  sprintf(buf, "%8x%8x%8x%8x", val1, val2, val3, val4);
  return String(buf);
}

// callback for MQTT subscriptions
void callback(char* topic, byte* payload, unsigned int length) 
{
    String topicStr = (String)topic;
    topicStr.toUpperCase();
    String payloadStr = (String)((char*)payload);
    payloadStr.remove(length);
    if (topicStr.startsWith(F("$IOTHUB/METHODS/POST/"))) { // direct method callback
        handleDirectMethod(topicStr, payloadStr);
    } else if (topicStr.indexOf(F("/MESSAGES/DEVICEBOUND/")) > -1) { // cloud to device message
        handleCloud2DeviceMessage(topicStr, payloadStr);
    } else if (topicStr.startsWith(F("$IOTHUB/TWIN/PATCH/PROPERTIES/DESIRED"))) {  // digital twin desired property change
        handleTwinPropertyChange(topicStr, payloadStr);
    } else if (topicStr.startsWith(F("$IOTHUB/TWIN/RES"))) { // digital twin response

        int result = atoi(topicStr.substring(topicStr.indexOf(F("/RES/")) + 5, topicStr.indexOf(F("/?$"))).c_str());
        int msgId = atoi(topicStr.substring(topicStr.indexOf(F("$RID=")) + 5, topicStr.indexOf(F("$VERSION=")) - 1).c_str());
        if (msgId == twinRequestId) 
        {
            Serial.println("");
            Serial.println("[loop] Device Twin Update Requested");
            updatePropertiesNow = true;

            // twin request processing
            twinRequestId = -1;

            gsm.readGSMInfo(); // get the imei and iccid

            // output limited to 128 bytes so this output may be truncated
            Serial.println("[IoT] Current state of device twin: " + String(payloadStr.c_str()));

            // read the property values sent using JSON parser
            JSON_Value *root_value = json_parse_string(payloadStr.c_str());
            JSON_Object *root_obj = json_value_get_object(root_value);
            JSON_Object* desireObj = json_object_get_object(root_obj, "desired");

            // append to local variables
            char* keys[] = {"dataCollectionInterval", "sampleValue", "telemetrySendInterval", "propertySendInterval", "deviceMode"};
            double constVals[] = {20000, 20, 50000, 5000, true}; // default vals for variables
            double arr[4];
            bool val4;

            for(int i = 0; i < 4; i++)
            {
                double localVal = json_object_get_number(desireObj, keys[i]);
                if(localVal > 2000000000) // check if the number is too big
                {
                    Serial.println("[IoT] Error - The number provided through the portal is too big to store");
                    arr[i] = constVals[i];
                }
                else
                {
                    arr[i] = localVal;
                }
                
            }
            val4 = json_object_get_boolean(desireObj, keys[4]);

            // append verified values to globals
            dataCollectionInterval = arr[0];
            sampleValue = arr[1];
            telemetrySendInterval = arr[2];
            propertySendInterval = arr[3];
            deviceMode = val4;

            // print these values
            Serial.println("      dataCollectionInterval  : " + String(dataCollectionInterval));
            Serial.println("      sampleValue             : " + String(sampleValue));
            Serial.println("      telemetrySendInterval   : " + String(telemetrySendInterval));
            Serial.println("      propertySendInterval    : " + String(propertySendInterval));
            Serial.println("      deviceMode              : " + String(deviceMode));

            String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
            char buff[20];
            topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
            String payload = basePropertyPayload;

            payload.replace("{sn}", chipId());
            payload.replace("{imei}", gsm.imei);
            
            servicesStatus[MQTT_STATUS] = TRANSMITTING;  ;
            Serial.println("[IoT] Publishing full twin: " + String(payload.c_str()));

            mqtt_client->publish(topic.c_str(), payload.c_str());
            servicesStatus[MQTT_STATUS] = CONNECTED;  ;
            requestId++;
            neo.show(TWIN_RECEIVE, 0, 0);

        } else {
            if (result >= 200 && result < 300) {
                Serial.println("[IoT] Twin property " + String(msgId) + " updated");
            } else {
                Serial.println("[IoT] Twin property " + String(msgId) + "update failed - err: " + String(result));
            }
            Serial.println("");
        }
    } else { // unknown message
        Serial.println("[IoT] Unknown message arrived [" + String(topic) + "]Payload contains: " + String(payloadStr.c_str()));
    }
}

// connect to Azure IoT Hub via MQTT
void connectMQTT(String deviceId, String username, String password) 
{
    mqttSslClient.stop(); // force close any existing connection
    servicesStatus[MQTT_STATUS] = DISCONNECTED;
     

    Serial.println("[IoT] Starting IoT Hub connection");
    int retry = 0;
    while(!mqtt_client->connected()) {     
        servicesStatus[MQTT_STATUS] = PENDING;
        Serial.println("[IoT] MQTT connection attempt #" + String(retry + 1));
         
        if (mqtt_client->connect(deviceId.c_str(), username.c_str(), password.c_str())) {
                Serial.println("[IoT] IoT Hub connection successful");
                mqttConnected = true;
        } else {
            servicesStatus[MQTT_STATUS] = DISCONNECTED;
            Serial.println("[IoT] IoT Hub connection error. MQTT rc=" + String(mqtt_client->state()));

            neo.show(LOOP_ERROR, 0, 0);
            delay(500);
            neo.show(SETUP_PROGRESS, 9, 12);
            delay(2000);
            retry++;
        }

        if(retry > 5)
        {
            neo.show(DEV_ERROR, 0, 0);
            while(true);
        }
    }

    if(mqtt_client->connected()) {
        servicesStatus[MQTT_STATUS] = CONNECTED;
         
        // add subscriptions
        mqtt_client->subscribe(IOT_TWIN_RESULT_TOPIC);  // twin results
        mqtt_client->subscribe(IOT_TWIN_DESIRED_PATCH_TOPIC);  // twin desired properties
        String c2dMessageTopic = IOT_C2D_TOPIC;
        c2dMessageTopic.replace(F("{device_id}"), deviceId);
        mqtt_client->subscribe(c2dMessageTopic.c_str());  // cloud to device messages
        mqtt_client->subscribe(IOT_DIRECT_MESSAGE_TOPIC); // direct messages
    } else {
        servicesStatus[MQTT_STATUS] = DISCONNECTED;
         
    }
}

// create an IoT Hub SAS token for authentication
String createIotHubSASToken(char *key, String url, long expire)
{
    Serial.println("");
    Serial.println("[IoT] Creating IoT Hub SAS Token");

    url.toLowerCase();
    String stringToSign = url + "\n" + String(expire);
    int keyLength = strlen(key);

    int decodedKeyLength = base64_dec_len(key, keyLength);
    char decodedKey[decodedKeyLength];

    base64_decode(decodedKey, key, keyLength);

    Sha256 *sha256 = new Sha256();
    sha256->initHmac((const uint8_t*)decodedKey, (size_t)decodedKeyLength);
    sha256->print(stringToSign);
    char* sign = (char*) sha256->resultHmac();
    int encodedSignLen = base64_enc_len(HASH_LENGTH);
    char encodedSign[encodedSignLen];
    base64_encode(encodedSign, sign, HASH_LENGTH);
    delete(sha256);

    return (char*)F("SharedAccessSignature sr=") + url + (char*)F("&sig=") + urlEncode((const char*)encodedSign) + (char*)F("&se=") + String(expire);
}