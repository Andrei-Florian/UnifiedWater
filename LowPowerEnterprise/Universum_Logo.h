// Universum | Universum Libraries > Logo

// Andrei Florian 7/SEP/2018

void logoStart(String projectName)
{
    delay(200);
    Serial.println("");
    
    delay(100);
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("        / / / /         ");
    
    Serial.println("");
    Serial.println("       Universum");
    Serial.println("  Expanding Boundaries");
    Serial.println("");
    
    delay(200);
    Serial.println(projectName);
    
    Serial.println("");
    Serial.println("");
}

void logoStartTFT(String projectName, int colour)
{
#ifndef ARDUINO_MKR
#include <Universum_TFTColours.h>
#include <Elegoo_GFX.h>    // Elegoo Core graphics library
#include <Elegoo_TFTLCD.h> // Elegoo Hardware-specific library
#include <SPI.h>
    
#define LCD_RD   A0
#define LCD_WR   A1
#define LCD_CD   A2
#define LCD_CS   A3
#define LCD_RESET A4
    
    delay(200);
    Serial.println("");
    
    delay(100);
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("    / /          / /    ");
    Serial.println("        / / / /         ");
    
    Serial.println("");
    Serial.println("       Universum");
    Serial.println("  Expanding Boundaries");
    Serial.println("");
    
    delay(200);
    Serial.println(projectName);
    
    Serial.println("");
    Serial.println("");
    
    Serial.println("Initialising TFT LCD");
    
    Serial.println("    OK - Creating Instance");
    Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET); // start the TFT LCD
    
    Serial.println("    OK - Initialising TFT LCD");
    tft.reset(); // reset the TFT
    delay(500); // delay for stability
    uint16_t identifier = tft.readID();
    identifier = 0x9341;
    tft.begin(identifier); // begin the TFT
    tft.setRotation(0);
    tft.fillScreen(WHITE); // backgroung colour
    delay(1000);
    
    Serial.println("    OK - Displaying Launch Screen");
    tft.setCursor(35,110); // set the cursor
    tft.setTextColor(BLUE); // set the colour of the text
    tft.setTextSize(3); // set the size of the text
    tft.println("Universum");
    
    tft.setCursor(60,140); // set the cursor
    tft.setTextColor(BLACK); // set the colour of the text
    tft.setTextSize(1); // set the size of the text
    tft.println("Expanding Boundaries");
    
    delay(4000);
    tft.fillScreen(colour); // backgroung colour
    Serial.println("    OK - Launch Screen Done");
    
    Serial.println("    Success - All Done");
    Serial.println("");
    Serial.println("");
#endif
}
