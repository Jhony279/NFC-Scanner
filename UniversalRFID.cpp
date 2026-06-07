#include "UniversalRFID.h"

UniversalRFID::UniversalRFID(byte ssPin, byte rstPin) : rfid(ssPin, rstPin) {
    userSlotPage = 3;   // Default page for user data (e.g., player name)
    creditSlotPage = 5; // Default page for credits
    rankSlotPage = 8;   // Default page for rank
}

// @brief Initializes the RFID reader and sets up the default key for authentication.
//      Must be called in setup() before any other operations.
void UniversalRFID::begin() {
    SPI.begin();      
    rfid.PCD_Init();  
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF; // Default factory key
    }
}

// @brief Scans for a new RFID tag and stores the tag type.
// @returns `true:` if a new tag is successfully detected and read. `false:` otherwise.
bool UniversalRFID::scanForTag() {
    if (!rfid.PICC_IsNewCardPresent()) return false;
    if (!rfid.PICC_ReadCardSerial()) return false;
    
    currentTagType = rfid.PICC_GetType(rfid.uid.sak);
    return true;
}


// @brief Halts the currently active tag and stops encryption.
// @note Should be called before scanning for a new tag or ending the program.
void UniversalRFID::haltTag() {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}

// @brief Sets the page numbers for user data, credits, and rank based on the tag type.
// @param uSlot The page number for user data (e.g., player name).
// @param cSlot The page number for credits.
// @param rSlot The page number for rank.
void UniversalRFID::setPages(int uSlot, int cSlot, int rSlot) {
    userSlotPage = uSlot;
    creditSlotPage = cSlot;
    rankSlotPage = rSlot;
}

// @brief Retrieves the UID of the currently active tag as a hexadecimal string.
// @return A string representing the UID of the active tag in hexadecimal format (e.g., "04AABBCCDD").
String UniversalRFID::getUID() {
    String uidString = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uidString += "0";
        uidString += String(rfid.uid.uidByte[i], HEX);
    }
    uidString.toUpperCase();
    return uidString;
}

// @brief Retrieves the type of the currently active tag as a string. 
// @returns A string representing the tag type (e.g., "MIFARE 1K", "NTAG215"). If the tag type is unknown, it returns "Unknown".
String UniversalRFID::getTagType() {
    return String(rfid.PICC_GetTypeName(currentTagType));
}

// ==========================================
//          DYNAMIC ROUTING
// ==========================================

// @brief Writes a string of data to the currently active tag. The slot number is translated into the appropriate 
//      memory location based on the tag type (MIFARE or NTAG).
// @param slotNumber The slot in the tag to write to (1-based index).
// @param data The string data to write to the tag. Max length is 48 bytes.
// @note `MIFARE` valid slots are 1-15. `NTAG` valid slots are 1-10.
// @return `true:` Write operation was successful. `false:` There was an error (e.g., invalid slot number, unsupported tag type, authentication failure).
bool UniversalRFID::writeData(int slotNumber, String data) {
    // 1. Universal Safety Check
    if (slotNumber < 1) {
        Serial.println("Error: Slot number must be 1 or higher.");
        return false;
    }

    // 2. Prepare the Payload
    byte payload[LARGE_PAYLOAD_SIZE];
    stringTo48ByteArray(data, payload);

    // 3. Hardware-Specific Routing & Translation
    if (currentTagType == MFRC522::PICC_TYPE_MIFARE_1K) {
        if (slotNumber > 15) {
            Serial.println("Error: MIFARE 1K only has 15 writable slots (1-15).");
            return false;
        }
        // MIFARE: Slot 1 equals Sector 1
        return writeMifare(slotNumber, payload);
        
    } else if (currentTagType == MFRC522::PICC_TYPE_MIFARE_UL) {
        if (slotNumber > 10) {
            Serial.println("Error: NTAG215 only holds 10 full 48-byte slots (1-10).");
            return false;
        }
        // NTAG215: Translate slot into a starting page (1->4, 2->16, 3->28...)
        int startPage = 4 + ((slotNumber - 1) * 12);
        return writeNTAG(startPage, payload);
    }
    
    Serial.println("Error: Unsupported Tag for Writing.");
    return false;
}

// @brief Reads the data stored on the currently active tag. The slot number is translated into the appropriate 
//      memory location based on the tag type (MIFARE or NTAG).
// @param slotNumber The slot in the tag to read from (1-based index).
// @return A string containing the data from the specified slot, or an empty string if there was an error.
// @note Possible errors: (e.g., invalid slot number, unsupported tag type, authentication failure).
String UniversalRFID::readData(int slotNumber) {
    if (slotNumber < 1) {
        Serial.println("Error: Slot number must be 1 or higher.");
        return "";
    }

    if (currentTagType == MFRC522::PICC_TYPE_MIFARE_1K) {
        if (slotNumber > 15) return "";
        return readMifare(slotNumber);
        
    } else if (currentTagType == MFRC522::PICC_TYPE_MIFARE_UL) {
        if (slotNumber > 10) return "";
        return readNTAG(slotNumber);
    }
    
    return "";
}

// @brief Prints the data stored in a specific slot on the currently active tag to the Serial Monitor. 
//      Reading the data as a String and then printing it formatted.
// @param slotNumber The slot number to read and print (1-based index).
// @note If there is an error during reading, it will print an appropriate error message instead of the data.
void UniversalRFID::printData(int slotNumber) {
    String data = readData(slotNumber); // Fetch the string
    
    // Only print if the string isn't empty (meaning the read was successful)
    if (data != "") {
        Serial.print(" -> Slot "); Serial.print(slotNumber); Serial.print(" Data: '");
        Serial.print(data);
        Serial.println("'");
    }
}

// @brief Prints all data stored in all available slots on the currently active tag to the Serial Monitor. 
//      Providing a comprehensive view of the tag's contents in a formatted maner.
// @note Iterates through all valid slot numbers based on the tag type and calls printData for each slot.
void UniversalRFID::printAllData() {
    Serial.println("\n========== FULL MEMORY DUMP ==========");
    
    if (currentTagType == MFRC522::PICC_TYPE_MIFARE_1K) {
        for(int slot = 1; slot <= 15; slot++) {
            printData(slot); // Uses our updated printData function
        }
    } else if (currentTagType == MFRC522::PICC_TYPE_MIFARE_UL) {
        for(int slot = 1; slot <= 10; slot++) {
            printData(slot); 
        }
    }
    
    Serial.println("======================================");
}

// @brief Updates the player's name stored on the tag.
// @param newName The new name to be stored on the tag.
// @return The updated name if the operation was successful, or an empty string if there was an error.
String UniversalRFID::updateName(String newName) {
    String currentData = readData(userSlotPage); // Read the current data from the user slot
    
    if (currentData == "") {
        Serial.println("Error: Unable to read current data for editing.");
        return "";
    }

    // Write the updated data back to the tag
    if (writeData(userSlotPage, newName)) {
        Serial.println("Player name updated successfully!");
        return newName;
    } else {
        Serial.println("Error: Failed to update player name.");
        return "";
    }
}

// @brief Updates the player's rank stored on the tag.
// @param newCredits credits to be added to the scanned tag.
// @return The credits added to the tag
String UniversalRFID::updateCredits(int newCredits) {
    String currentData = readData(creditSlotPage); // Read the current data from the credit slot
    
    if (currentData == "") {
        Serial.println("Error: Unable to read current data for editing.");
        return "";

    }
    
    Serial.print("Current Credits: ");
    Serial.println(currentData);

    // Conversion from String to int for manipulation
    int currentCredits = currentData.toInt();
    currentCredits += newCredits; // Update the credits to the new value
    String sCredits = String(currentCredits); // Convert back to String for writing
    
    // Write the updated data back to the tag
    if (writeData(creditSlotPage, sCredits)) {
        Serial.println("Credits updated successfully!");
        return sCredits;
    } else {
        Serial.println("Error: Failed to update credits.");
        return "";
    }
}

// ==========================================
//          HARDWARE SPECIFIC LOGIC
// ==========================================

// @brief Writes a 48-byte payload to a specified MIFARE sector (slotNumber). Each sector consists of 4 blocks, 
//      and the function writes 16 bytes to writeable block. 
// @param slotNumber The slot number to write to (1-based index).
// @param dataToWrite A pointer to the data to be written.
// @return `true:` All blocks were written successfully `false:` There was an authentication failure or write error.
bool UniversalRFID::writeMifare(int slotNumber, byte* dataToWrite) {
    int firstBlock = slotNumber * 4;
    MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, firstBlock, &key, &(rfid.uid));
    
    // VISUAL FEEDBACK: Print if the password fails
    if (status != MFRC522::STATUS_OK) {
        Serial.print(" -> Write Auth Failed: ");
        Serial.println(rfid.GetStatusCodeName(status));
        delay(ERROR_DELAY_MS); 
        return false;      
    } 
    
    for (int i = 0; i < 3; i++) {
        status = rfid.MIFARE_Write(firstBlock + i, &dataToWrite[i * 16], 16); 
        if (status != MFRC522::STATUS_OK) {
            Serial.print(" -> Write Failed on Block ");
            Serial.println(firstBlock + i);
            return false;
        }
    }
    
    Serial.println(" -> Write Successful!"); // VISUAL FEEDBACK
    return true;
}

// @brief Reads a 48-byte payload from a specified MIFARE sector (slotNumber). Each sector consists of 4 blocks, and the function 
//      reads 16 bytes from each writeable block.
// @param slotNumber The slot number to read from (1-based index).
// @return A string containing the data from the specified slot, or an empty string if there was an error.
// @note - Prints out error messages to the Serial Monitor if authentication or reading fails.
// @note - Possible errors: authentication failure, read error on any block.
String UniversalRFID::readMifare(int slotNumber) {
    int firstBlock = slotNumber * 4; 
    byte readBuffer[18]; 
    byte size = sizeof(readBuffer);
    byte largePayload[LARGE_PAYLOAD_SIZE]; 
    
    MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, firstBlock, &key, &(rfid.uid));
    
    if (status != MFRC522::STATUS_OK) {
        Serial.print(" -> Read Auth Failed: ");
        Serial.println(rfid.GetStatusCodeName(status));
        return ""; 
    }      
    
    for (int i = 0; i < 3; i++) {
        status = rfid.MIFARE_Read(firstBlock + i, readBuffer, &size);
        if (status == MFRC522::STATUS_OK) {
            for (int j = 0; j < 16; j++) largePayload[(i * 16) + j] = readBuffer[j];
        } else {
            Serial.print(" -> Read Failed on Block ");
            Serial.println(firstBlock + i);
            return "";
        }
    }
    
    // Convert the array to a String and return it
    return payloadToString(largePayload, LARGE_PAYLOAD_SIZE);
}

// @brief Writes a 48-byte payload to a specified NTAG page (startPage). Each page consists of 4 bytes, 
//      and the function writes 4 bytes to each page. 
// @param startPage The starting page number to write to (e.g., 4 for slot 1, 16 for slot 2, etc.).
// @param dataToWrite A pointer to the data to be written.
// @return `true` All pages were written successfully `false` There was an authentication failure or write error.
bool UniversalRFID::writeNTAG(int startPage, byte* dataToWrite) {
    for (int i = 0; i < 12; i++) {
        MFRC522::StatusCode status = rfid.MIFARE_Ultralight_Write(startPage + i, &dataToWrite[i * 4], 4);
        if (status != MFRC522::STATUS_OK) {
            delay(ERROR_DELAY_MS);
            return false; 
        }
    }
    return true;
}

// @brief Reads a 48-byte payload from a specified NTAG page (startPage). Each page consists of 4 bytes, 
//      and the function reads 4 bytes from each page. 
// @param slotNumber The slot number to read from (1-based index).
// @return A string containing the data from the specified slot, or an empty string if there was an error.
// @note - Prints out error messages to the Serial Monitor if authentication or reading fails.
// @note - Possible errors: authentication failure, read error on any block.
String UniversalRFID::readNTAG(int slotNumber) {
    int startPage = 4 + ((slotNumber - 1) * 12);
    byte readBuffer[18]; 
    byte size = sizeof(readBuffer);
    byte largePayload[LARGE_PAYLOAD_SIZE]; 
    
    for (int i = 0; i < 3; i++) {
        MFRC522::StatusCode status = rfid.MIFARE_Read(startPage + (i * 4), readBuffer, &size);
        if (status == MFRC522::STATUS_OK) {
            for (int j = 0; j < 16; j++) largePayload[(i * 16) + j] = readBuffer[j];
        } else {
            return "";
        }
    }
    
    // Convert the array to a String and return it
    return payloadToString(largePayload, LARGE_PAYLOAD_SIZE);
}

// ==========================================
//          DATA FORMATTING HELPERS
// ==========================================

// @brief Converts a String into a 48-byte array, padding is added if the string is shorter than 48 characters. 
//      Used to prepare data for writing to both MIFARE and NTAG tags, which have different block/page sizes 
//      but can both accommodate a 48-byte payload when using multiple blocks/pages.
// @param input The string to be converted into a byte array.
// @param payload A pointer to a byte array where the converted data will be stored.
// @note If the input string exceeds 48 characters, it will be truncated to fit the payload size.
void UniversalRFID::stringTo48ByteArray(String input, byte* payload) {
    for (int i = 0; i < LARGE_PAYLOAD_SIZE; i++) {
        if (i < input.length()) {
            payload[i] = (byte)input.charAt(i);
        } else {
            payload[i] = ' '; 
        }
    }
}

// @brief Converts a byte array into a human-readable String. Iterates through the byte array and constructs a String, 
//      replacing non-printable characters with dots. Used to display the contents of the tag in a readable format.
// @param buffer A pointer to the byte array to be converted.
// @param length The number of bytes to convert.
// @return A string containing the human-readable data.
String UniversalRFID::payloadToString(byte* buffer, int length) {
    String result = "";
    for (int i = 0; i < length; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            result += (char)buffer[i]; // Append valid characters
        } else {
            result += ".";             // Replace hidden bytes with dots
        }
    }
    return result;
}

// ==========================================
//          OLED SCREEN UI LOGIC
// ==========================================

// @brief Attaches an Adafruit_SSD1306 OLED display to the UniversalRFID library, allowing it to control 
//      the screen for displaying information.
// @param displayOled A pointer to an initialized Adafruit_SSD1306 object that represents the OLED screen.
void UniversalRFID::attachDisplay(Adafruit_SSD1306* displayOled) {
    oled = displayOled; // Store the memory address of the screen
}

// @brief Displays the idle screen on the attached OLED, showing a "System Ready" message and prompting the user to scan a tag.
// @note If no screen is attached, the function will simply return without doing anything.
void UniversalRFID::showIdleScreen() {
    // Safety check: Don't do anything if no screen is attached
    if (oled == nullptr) return; 

    oled->clearDisplay();
    oled->setTextSize(1);
    oled->setTextColor(SSD1306_WHITE);
    oled->setCursor(0, 0);
    
    oled->println("System Ready.");
    oled->println("Waiting for tag...");
    
    oled->display();
}

// @brief Displays information of user to screen on the attached OLED, showing the data from a specific slot on the tag in a formatted manner.
// @param slotNumber The slot number to read from and display (1-based index).
// @note An RFID tag must be scanned and active for this function to work.
void UniversalRFID::displayToOled(int slotNumber) {
    if (oled == nullptr) return;

    // Get the raw data from the tag for the specified slot
    String userName = readData(userSlotPage);
    String credits = readData(creditSlotPage);
    String rank = readData(rankSlotPage);

    // 2. Find the labels
    int playerStart = userName.length() > 0 ? 0 : -1; // If userName is empty, set to -1
    int creditsStart = credits.toInt() ? 0 : -1; // If credits is empty or non-numeric, set to -1
    int rankStart = rank.length() > 0 ? 0 : -1; // If rank is empty, set to -1

    // 3. Draw the UI Frame
    oled->clearDisplay(); 
    oled->setCursor(0,0); 
    oled->println("User Info:");
    oled->println("---------------------");

    // 4. Slice and draw the data rows
    if (playerStart != -1 && creditsStart != -1 && rankStart != -1) {
        String playerLine = userName;
        String creditsLine = credits;
        String rankLine = rank; 

        oled->setTextSize(1);
        oled->setTextColor(SSD1306_WHITE);
        oled->println(playerLine);
        oled->println(creditsLine);
        // oled->println(rankLine); for now
    } else {
        // Fallback for empty or corrupted tags
        // oled->println("UNKNOWN DATA FORMAT/CARD REMOVED TOO SOON");
        oled->println("UNKNOWN DATA FORMAT");
        oled->println(userName);
        oled->println(credits);
        oled->println(rank);
    }
    
    oled->display(); // Push to the physical screen
}

// @brief Prints a custom message to the attached OLED screen. Clears the screen before displaying the new message.
// @param message The string message to be displayed on the OLED screen.
void UniversalRFID::printToOled(String message) {
    if (oled == nullptr) return;

    oled->clearDisplay();
    oled->setCursor(0,0);
    oled->println(message);
    oled->display();
}

// @brief Displays cached user data (name, credits, rank) on the attached OLED screen. 
// This is used to show information even when the card is no longer present.
// @param name The player's name to display.
// @param credits The player's credits to display.
// @param rank The player's rank to display.
// @note This function is intended to be used when the card has been removed, so it does not attempt to read from the tag and instead relies on passed-in data.
void UniversalRFID::displayCachedData(String name, String credits, String rank) {
    if (oled == nullptr) return;

    oled->clearDisplay(); 
    oled->setCursor(0,0); 
    oled->setTextSize(1);
    oled->setTextColor(SSD1306_WHITE);
    
    oled->println("User Info:");
    oled->println("---------------------");

    // Print the passed variables
    oled->print("PLAYER: "); oled->println(name);
    oled->print("CREDITS: "); oled->println(credits);
    oled->print("RANK: "); oled->println(rank);
    
    oled->display(); 
}