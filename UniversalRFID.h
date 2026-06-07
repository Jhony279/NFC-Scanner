#ifndef UNIVERSAL_RFID_H
#define UNIVERSAL_RFID_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class UniversalRFID {
    public:
        // Constructor
        UniversalRFID(byte ssPin, byte rstPin);

        void begin();
        bool scanForTag();
        void haltTag();
        void setPages(int uSlot, int cSlot, int rSlot);

        String getUID();
        String getTagType();

        bool writeData(int slotNumber, String data);
        void printData(int slotNumber);
        void printAllData();
        String readData(int slotNumber);

        void updateCredits(int newCredits);
        void updateName(String newName);
        void updateRank(String newRank);

        void attachDisplay(Adafruit_SSD1306* displayOled); 
        
        void showIdleScreen();
        void displayToOled(int slotNumber);
        void printToOled(String message);

    private:
        MFRC522 rfid;
        MFRC522::MIFARE_Key key;
        MFRC522::PICC_Type currentTagType;
        
        // Constants
        const byte LARGE_PAYLOAD_SIZE = 48;
        const int ERROR_DELAY_MS = 1000;

        // Pages
        int userSlotPage;
        int creditSlotPage;
        int rankSlotPage;

        // A pointer to hold our attached screen
        Adafruit_SSD1306* oled = nullptr;

        void stringTo48ByteArray(String input, byte* payload);
        String payloadToString(byte* buffer, int length);

        bool writeMifare(int sectorNum, byte* dataToWrite);
        bool writeNTAG(int startPage, byte* dataToWrite);
        String readMifare(int slotNumber);
        String readNTAG(int slotNumber);
};

#endif