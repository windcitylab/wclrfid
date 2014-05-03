#include <SPI.h>
#include <boards.h>
#include <ble_shield.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_NFCShield_I2C.h>
#include "LinkedListEEPROM.h"

// Command from iPhone
#define LIST_RECORDS 0x01
#define ADD_RECORD 0x02
#define DELETE_RECORD 0x03
#define READ_RFID_TAG 0x04
#define ENTER_COMMAND_MODE 0x05
#define ENTER_NORMAL_OPERATION_MODE 0x06
#define STATUS_CHECK 0xFF

// Commands going to the iPhone
#define SENDING_RECORD_COUNT 0x01
#define SENDING_CURRENT_SLOT_NUMBER 0x02
#define SENDING_REFRESH_REQUEST 0x03
#define SENDING_GOOD_TAG_STATUS 0x04
#define SENDING_BAD_TAG_STATUS 0x05
#define SENDING_ENTERING_COMMAND_MODE 0x06
#define SENDING_ENTERING_NORMAL_OPERATION_MODE 0x07

#define IRQ   (2)
#define RESET (3)  // Not connected by default on the NFC Shield

Adafruit_NFCShield_I2C nfc(IRQ, RESET);
uint8_t success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

LinkedListEEPROM myEEPROM;

bool inCommandMode = true;

//void retrieveRecordAtSlot(int slotNumber, Record &record);
void sendRecordToiPhone(Record record);
bool rfidTagValid(byte rfidTag[RFID_TAG_LENGTH], Record &record);

void sendRecordToiPhone(Record record)
{
  unsigned char sn[2] = {SENDING_CURRENT_SLOT_NUMBER,record.slotNumber};
  ble_write_bytes(sn,2);
  ble_do_events();
  ble_write_bytes(record.rfidTag,RFID_TAG_LENGTH);
  ble_do_events();
  ble_write_bytes(record.name,RECORD_NAME_LENGTH);
  ble_do_events();
}
bool rfidTagValid(byte rfidTag[RFID_TAG_LENGTH], Record &record)
{
    int slotNumber = myEEPROM.retrieveFirstSlotNumber();
    bool found = false;
    while ((slotNumber != 0xFF) & (found == false))
    {
      myEEPROM.retrieveRecordAtSlot(slotNumber,record);
      found = true;
      for (int i=0; i< RFID_TAG_LENGTH; i++)
      {
        if (record.rfidTag[i] != rfidTag[i])
        {
          found = false;
          break;
        }
      }
      slotNumber = record.nextSlotNumber;
    } 
   return found; 
}
void sendMessageToiPhone(unsigned char uid[RFID_TAG_LENGTH])
{
  Record record;
  if (rfidTagValid(uid,record))
  {
    ble_write(SENDING_GOOD_TAG_STATUS);
    ble_do_events();
    ble_write_bytes(uid,RFID_TAG_LENGTH);
    ble_do_events();
    ble_write_bytes(record.name,RECORD_NAME_LENGTH);
    ble_do_events();
    Serial.println("Tag is valid!");
  }
  else 
  {
    ble_write(SENDING_BAD_TAG_STATUS);
    ble_do_events();
    ble_write_bytes(uid,RFID_TAG_LENGTH);
    ble_do_events();
    Serial.println("Tag is not valid!");
  }
}

unsigned int time, watchDogTimer;

void setup() {
  Serial.begin(115200);
  nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  Serial.println("Waiting for an ISO14443A Card ...");

  ble_begin();
  ble_do_events();
  time = 1;
  watchDogTimer = 0;
}

void enterNormalOperationMode()
{
  inCommandMode = false;
  ble_write(SENDING_ENTERING_NORMAL_OPERATION_MODE);
  ble_do_events();
}

void loop() {
  ble_do_events();
  time++;
  if (time == 0)
  {
    watchDogTimer++;
    if (watchDogTimer == 10)
    {
      if (inCommandMode)
        enterNormalOperationMode();
    }
  }
  if (ble_available() > 0)
  {
    if (inCommandMode)
    {
      watchDogTimer = 0;
      int value = ble_read();
      if (value == STATUS_CHECK)
      {
        ble_write(STATUS_CHECK);
        ble_do_events();
      }
      if (value == ENTER_NORMAL_OPERATION_MODE) 
      {
        enterNormalOperationMode();
      }
      if (value == LIST_RECORDS)
      {
        int numberOfRecords = myEEPROM.recordCount();
        unsigned char sn[2] = {SENDING_RECORD_COUNT,numberOfRecords};
        ble_write_bytes(sn,2);
        ble_do_events();
        
        int slotNumber = myEEPROM.retrieveFirstSlotNumber();
        Record record;
        while (slotNumber != 0xFF)
        {
          myEEPROM.retrieveRecordAtSlot(slotNumber,record);
          sendRecordToiPhone(record);
          slotNumber = record.nextSlotNumber;
        }
      }
      if (value == ADD_RECORD)
      {
        Serial.println("Waiting for RFID to add to DB");
         success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
         char name[RECORD_NAME_LENGTH];
         for (int i=0; i<RECORD_NAME_LENGTH; i++)
         {
           if (ble_available() > 0)
           {
             name[i] = ble_read();
           }
           else 
           {
             name[i] = 0;
           }
         }
         myEEPROM.addRecord(uid,name);
         ble_write(SENDING_REFRESH_REQUEST);
         ble_do_events();
      }
      if (value == DELETE_RECORD)
      {
         char slotNumber = ble_read();
         myEEPROM.deleteRecord(slotNumber);
         ble_write(SENDING_REFRESH_REQUEST);
         ble_do_events();
      }
      if (value == READ_RFID_TAG)
      {
        Serial.println("Waiting for RFID Scan...");
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
        nfc.PrintHex(uid, uidLength);
        sendMessageToiPhone(uid);
      }
    }
    else 
    {
      int value = ble_read();
      if (value == ENTER_COMMAND_MODE)
      {
        inCommandMode = true;
        ble_write(SENDING_ENTERING_COMMAND_MODE);
        ble_do_events();
      }
    }
  }
  else 
  {
    if (!inCommandMode) {
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
      nfc.PrintHex(uid, uidLength);   
      if (ble_available() == 0)
      {
        sendMessageToiPhone(uid);
        delay(250);
      } 
    }
  }
}
