// User Settings
String deviceId = ""; // the ID of the device

bool deviceMode = false;                            // false for personal, true for enterprise
double telemetrySendInterval  = 50000;              // the rate at which data is sent to the backend
double dataCollectionInterval = 20000;              // the rate at which the data is collected from the sensors
double propertySendInterval = 5000;                 // the rate at which properties are sent
double keepConnectionAlive = 1000000;               // the time to keep the connection to backend alive for (has to be greater than sleep)
int sampleValue = 20;                               // the number of samples to take from inprecise sensors

int timeZone = 1;                                   // difference from GMT
bool updatePropertiesNow = false;

// calibration variables
double turbidityCalibration = 2.30;                 // the turbidity as a voltage in distilled water
double phCalibration = 2.3;                         // the offset of the device from 7 when placed in distilled water
int sampleDelayTime = 1000;                         // the time to wait between reads

// Azure IoT Central device information
static char PROGMEM iotc_enrollmentKey[]    = "";   // select administration from menu on left -> device connection -> SAS-IoT-Devices -> Primary Key
static char PROGMEM iotc_scopeId[]          = "";   // the specific device ID (click on connect -> ID Scope)
static char PROGMEM iotc_modelId[]          = "";   // custom device name (click on connect -> Device ID field)

// GSM information
static char PROGMEM PINNUMBER[]     = "";

// APN data
static char PROGMEM GPRS_APN[]      = "Hologram";
static char PROGMEM GPRS_LOGIN[]    = "";
static char PROGMEM GPRS_PASSWORD[] = "";

// Connection Pins for sensors
int pHPin = A1;
int turbidityPin = A2;
int neopixelPin = 4;
int waterTempPin = 3;
int buttonPin = 2;

// Device properties being sent
#define DEVICE_PROP_FW_VERSION          "1.0.0-20190704"
#define DEVICE_PROP_MANUFACTURER        "Arduino"
#define DEVICE_PROP_PROC_MANUFACTURER   "Microchip"
#define DEVICE_PROP_PROC_TYPE           "SAMD21"
#define DEVICE_PROP_DEVICE_MODEL        "MKR1400 GSM"

#define PERSONAL_MODE       false
#define ENTERPRISE_MODE     true

int DEV_ERROR          = 0;
int LOOP_ERROR         = 1;
int SETUP_PROGRESS     = 2;
int COLLECT_PROGRESS   = 3;
int SEND_PROGRESS      = 4;
int WAKEUP             = 5;
int TWIN_RECEIVE       = 6;

// payload templates
String personalTelemetryPayload = 
"{"
    "{LOCATION}, "
    "\"waterPh\": {waterPh}, \"waterTurbidity\": {waterTurbidity}, \"waterTemp\": {waterTemp}, \"atmoTemp\": {atmoTemp}, \"atmoHumidity\": {atmoHumidity}"
"}";

String enterpriseTelemetryPayload = 
"{"
    "\"waterPh\": {waterPh}, \"waterTurbidity\": {waterTurbidity}, \"waterTemp\": {waterTemp}, \"atmoTemp\": {atmoTemp}, \"atmoHumidity\": {atmoHumidity}"
"}";

String propertyPayload = 
"{"
    "\"batteryRemaining\"           : {bat},                    " 
    "\"locationAccuracy\"           : {accuracy},               "
    "\"telemetrySendInterval\"      : {telemetrySendInterval},  "
    "\"sampleValue\"                : {sampleValue},            "
    "\"dataCollectionInterval\"     : {dataCollectionInterval}, "
    "\"propertySendInterval\"       : {propertySendInterval},   "
    "\"deviceMode\"                 : {deviceMode}              "
"}";

String basePropertyPayload = 
"{ "
    "\"manufacturer\": \""          DEVICE_PROP_MANUFACTURER         "\", "
    "\"model\": \""                 DEVICE_PROP_DEVICE_MODEL         "\", "
    "\"processorManufacturer\": \"" DEVICE_PROP_PROC_MANUFACTURER    "\", "
    "\"processorType\": \""         DEVICE_PROP_PROC_TYPE            "\", "
    "\"fwVersion\": \""             DEVICE_PROP_FW_VERSION           "\", "
    "\"serialNumber\": \""          "{sn}"                           "\", "
    "\"imei\": \""                  "{imei}"                         "\"  "
"}";