#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "UniversalRFID.h"
#include <Preferences.h>

// --- OLED Display Settings ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Set to -1 if your screen doesn't have a dedicated reset pin
#define SCREEN_ADDRESS 0x3C // 0x3C is the standard I2C address for 0.96" OLEDs

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define pins for ESP32 DevKit V1
#define SS_PIN  5  
#define RST_PIN 4 

UniversalRFID myReader(SS_PIN, RST_PIN);

void setup() {
    Serial.begin(115200);
    while (!Serial);

    myReader.begin();
    
    Serial.println("Hardware Agnostic System Ready.");
    Serial.println("Tap ANY tag...");

    // Initialize OLED
    // SSD1306_SWITCHCAPVCC tells the screen to generate its own 3.3V internal power
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("OLED connection failed! Check wiring.");
        for(;;); // Freeze the code here if the screen is dead
    }
    
    // MAGIC STEP: Hand the screen over to the library!
    myReader.attachDisplay(&display);

    // Tell the library to draw the waiting screen
    myReader.showIdleScreen();
    
    display.println("System Ready.");
    display.println("Waiting for tag...");
}

void loop() {
    // Check for a card
    if (!myReader.scanForTag()) return;

    Serial.println("\n-----------------------------");
    Serial.print("UID: ");
    Serial.println(myReader.getUID());
    Serial.print("Type: ");
    Serial.println(myReader.getTagType());
    
    // Build your dynamic string max 48 chars (fits in one slot)
    String accountInfo = "PLAYER:Johnathan CREDITS:150 RANK:PRO";
    // TODO: seperate name & credits into different slots
    
    // Write data to "Slot 1" (Auto-routes to Sector 1 or Page 4)
    Serial.println("Writing Data to Slot 1...");
    myReader.writeData(1, accountInfo); 
    
    // Read "Slot 1" back
    Serial.println("\nReading Slot 1...");
    myReader.printData(1); 
    
    // Print all data on the tag for verification
    Serial.println("All Data on Tag:");
    myReader.printAllData();
    
    // Grab the string directly from Slot 1
    String cardData = myReader.readData(1);

    // Automatically parse and draw Slot 1 to the OLED!
    myReader.showAccessGranted(1);
    
    // Safely halt the tag
    myReader.haltTag();
    delay(5000); 

    // Return to the waiting screen
    myReader.showIdleScreen();
}

// // Use it programmatically!
// if (cardData.indexOf("RANK:PRO") != -1) {
//     Serial.println("Welcome back, VIP User!");
//     // Turn on green LED...
// } else {
//     Serial.println("Standard Access.");
//     // Turn on standard LED...
// }