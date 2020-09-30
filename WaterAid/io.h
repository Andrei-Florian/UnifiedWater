#include <Wire.h>
#include "SparkFunHTU21D.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_NeoPixel.h>


OneWire oneWire(waterTempPin);
DallasTemperature soilSens(&oneWire);
HTU21D gy21;

const int pixels = 16;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixels, neopixelPin, NEO_GRB + NEO_KHZ800);

// functions
struct Neo
{
    public:
        void init()
        {
            Serial.println("[setup] Initialising LED Strip");
            strip.begin();
            strip.show();
        }

        void hide(bool animate)
        {
            if(deviceMode == PERSONAL_MODE)
            {
                delay(1000);
                
                if(animate)
                {
                    for(int i = 0; i <= pixels; i++)
                    {
                        strip.setPixelColor(i, strip.Color(0, 0, 0));
                        strip.show();
                        delay(20);
                    }
                }
                else
                {
                    strip.clear();
                }
            }
        }

        void show(int mode, int start, int end)
        {
            if(deviceMode == PERSONAL_MODE)
            {
                if(mode == DEV_ERROR || mode == LOOP_ERROR)
                {
                    for(int i = 0; i < pixels; i++)
                    {
                        strip.setPixelColor(i, strip.Color(255, 0, 0));
                        
                        strip.show();
                        delay(20);
                    }

                    if(mode == LOOP_ERROR)
                    {
                        delay(1000);
                        
                        for(int i = 0; i < pixels; i++)
                        {
                            strip.setPixelColor(i, strip.Color(0, 0, 0));
                            
                            strip.show();
                            delay(20);
                        }
                    }
                }
                else if(mode == SETUP_PROGRESS)
                {
                    for(int i = start; i < end; i++)
                    {
                        strip.setPixelColor(i, strip.Color(255, 255, 255));
                        strip.show();
                        delay(20);
                    }
                }
                else if(mode == COLLECT_PROGRESS)
                {
                    for(int i = start; i < end; i++)
                    {
                        strip.setPixelColor(i, strip.Color(0, 0, 255));
                        strip.show();
                        delay(20);
                    }
                }
                else if(mode == SEND_PROGRESS)
                {
                    for(int i = start; i < end; i++)
                    {
                        strip.setPixelColor(i, strip.Color(0, 255, 0));
                        strip.show();
                        delay(20);
                    }
                }
                else if(mode == TWIN_RECEIVE)
                {
                    for(int i = 0; i < pixels; i++)
                    {
                        strip.setPixelColor(i, strip.Color(255, 0, 255));
                        strip.show();
                        delay(20);
                    }

                    this->hide(true);
                }
                else // assume wakeup
                {
                    for(int i = 0; i < pixels; i++)
                    {
                        strip.setPixelColor(i, strip.Color(255, 255, 255));
                        strip.show();
                        delay(20);
                    }

                    this->hide(true);
                }
            }
        }
};

Neo neo;

struct Get
{
    private:
        double samplePh()
        {
            // raw value = value of pin A0
            int rawVal = analogRead(pHPin);

            double voltage = rawVal * 3.3 / 1024;
            double finalVal = 3.5 * voltage + phCalibration;

            return finalVal;
        }

        double sampleTurbidity()
        {
            double finalVal = analogRead(turbidityPin);
            double volt = finalVal / 1024.0 * 3.3;

            return volt;
        }

        double getNTU(double x)
        {
            // quadratic equation to extract the NTU value from the voltage inputted (x)
            return (sq(x)*-1120.4) + (5742.3 * x) - 4352.9;
        }

    public:
        void init()
        {
            Serial.println("[setup] Setting up Sensors and modules");
            // set up button pin as input
            pinMode(buttonPin, INPUT);

            // setup the gy21
            gy21.begin();

            // setup the waterproof temp sensor
            soilSens.begin();

            // start the neopixel
            Serial.println("[setup] Initialising the Neopixel ring");

            Serial.println("");
        }

        bool button()
        {
            if(digitalRead(buttonPin) == HIGH)
            {
                neo.show(COLLECT_PROGRESS, 0, pixels);

                while(digitalRead(buttonPin) == HIGH);
                neo.hide(true);
                return true;
            }
            return false;
        }

        double pH()
        {
            Serial.println("[loop] Getting water pH");
            double finalVal = 0;

            // take multiple samples from inprecise sensors and round
            for(int i = 0; i < sampleValue; i++)
            {
                finalVal += this->samplePh();
                delay(1000);
            }
            
            finalVal = finalVal / sampleValue;
            return finalVal;
        }

        double turbidity()
        {
            Serial.println("[loop] Getting Water turbidity");
            double volt = 0;
            double finalVal = 0;
            
            // take multiple samples from inprecise sensors and round
            for(int i = 0; i < sampleValue; i++)
            {
                volt += this->sampleTurbidity();
                delay(1000);
            }

            volt = volt / sampleValue;
            finalVal = this->getNTU(volt);

            return finalVal;
        }

        double temperature()
        {
            Serial.println("[loop] Getting water temperature");
            soilSens.requestTemperatures();
            return soilSens.getTempCByIndex(0);
        }

        struct GY21
        {
            double temperature()
            {
                Serial.println("[loop] Getting atmospheric temperature");
                return gy21.readTemperature();
            }

            double humidity()
            {
                Serial.println("[loop] Getting atmospheric humidity");
                return gy21.readHumidity();
            }
        };

        GY21 thSensor;
};

Get get;

struct Battery
{
    public:
        double get()
        {
            Serial.println("[loop] Getting battery voltage (if battery connected directly)");
            int sensorValue = analogRead(ADC_BATTERY);
            double voltage = sensorValue * (4.3 / 1023.0);
            return voltage;
        }
};

Battery battery;