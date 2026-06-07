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

    // Initialize OLED
    // SSD1306_SWITCHCAPVCC tells the screen to generate its own 3.3V internal power
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("OLED connection failed! Check wiring.");
        for(;;); // Freeze the code here if the screen is dead
    }
    
    // Hand the screen over to the adafruit library
    myReader.attachDisplay(&display);

    // Tell the library to draw the waiting screen
    myReader.showIdleScreen();
    
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
    
    // test string to write to the tag
    String accountInfo = "hkjkjk PLAYER:Pearssooon njnj CREDITS:11150 RANK:PRO";
    // TODO: seperate name & credits into different slots

    // Parse the string to extract the player's name and credits
    int nameStart = accountInfo.indexOf("PLAYER:") + 7;
    int creditsStart = accountInfo.indexOf("CREDITS:") + 8;
    int rankStart = accountInfo.indexOf("RANK:") + 5;
    String playerName = accountInfo.substring(nameStart, accountInfo.indexOf(" ", nameStart));
    String credits = accountInfo.substring(creditsStart, accountInfo.indexOf(" ", creditsStart));
    String rank = accountInfo.substring(rankStart, accountInfo.indexOf(" ", rankStart));

    // Read the old values
    String oldName = myReader.readData(3);
    String oldCredits = myReader.readData(5);
    String oldRank = myReader.readData(8);
    
    // Write data to tag
    Serial.println("Writing Data to Slot 1...");
    myReader.writeData(3, playerName);
    myReader.writeData(5, credits);
    myReader.writeData(8, rank);
    
    // Print all data on the tag for verification
    Serial.println("All Data on Tag:");
    myReader.printAllData();
    
    // Grab the string directly from Slot 1
    String cardData = myReader.readData(1);
    Serial.print("cardData: ");
    Serial.println(cardData);

    String newCredits = myReader.updateCredits(50); // Add 50 credits to the current value in slot 1
    myReader.updateName("Person"); // Update the player name in slot 1
    myReader.printAllData();
    
    // Safely halt the tag
    myReader.haltTag();
    Serial.println("Card halted. User can safely remove it.");

    // OLED SEQUENCE 
    if (newCredits != "") {
        // Show the formatted BEFORE state
        myReader.displayCachedData(oldName, oldCredits, oldRank);
        delay(2500); // Leave it on screen for 2.5 seconds

        // Show the transition message
        myReader.printToOled("\n   INFO UPDATED!   ");
        delay(1500); // Leave message for 1.5 seconds

        // Show the formatted AFTER state
        myReader.displayCachedData(oldName, newCredits, oldRank);
        delay(4000); // Leave final stats up for 4 seconds
        
    } else {
        // Fallback if the card was removed too early during step 3
        myReader.printToOled("Update Failed.\nTry Again.");
        delay(3000);
    }
    // Return to the waiting screen
    myReader.showIdleScreen();
}