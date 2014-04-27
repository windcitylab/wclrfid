#include <SPI.h>
#include <boards.h>
#include <ble_shield.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_NFCShield_I2C.h>

#define RECORD_NAME_LENGTH 20
#define RFID_TAG_LENGTH 4
#define RECORD_LENGTH 26
#define LRN 0x3FA // last record number
#define SRN 0x3F9 // starting record number
#define STATUS 0x3FB

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

typedef struct Record {
  int slotNumber;
  byte rfidTag[RFID_TAG_LENGTH];
  unsigned char name[RECORD_NAME_LENGTH];
  int nextSlotNumber;
} Record;

bool inCommandMode = true;

void retrieveRecordAtSlot(int slotNumber, Record &record);
void sendRecordToiPhone(Record record);
bool rfidTagValid(byte rfidTag[RFID_TAG_LENGTH], Record &record);

void writeName(char name[], int atAddress)
{
  for (int i=0; i<RECORD_NAME_LENGTH; i++)
  {
    if (i < strlen(name))
    {
      EEPROM.write(atAddress+1+RFID_TAG_LENGTH+i,name[i]);
    }
    else
    {
      EEPROM.write(atAddress+1+RFID_TAG_LENGTH+i,0);
    }
  }
}

void addRecord(byte rfidTag[], char name[])
{
  // get an open slot
  uint8_t newSlot = findOpenSlot();
  if (newSlot > 39) return;  // we have no empty slots remaining so just return

  markRecordNumberFree(newSlot,false);  // don't free it, essentially marks as taken
  
  // get last record
  uint8_t lastRecordNumber = EEPROM.read(LRN);
  if (lastRecordNumber == 0xFF)
  {
    // We don't have any records yet
    int addressOfRecord = newSlot * RECORD_LENGTH;
    EEPROM.write(addressOfRecord,0xFF);
    for (int i=0; i<RFID_TAG_LENGTH; i++)
    {
      EEPROM.write(addressOfRecord+1+i,rfidTag[i]);
    }
    writeName(name,addressOfRecord);
    EEPROM.write(addressOfRecord+RFID_TAG_LENGTH+RECORD_NAME_LENGTH+1,0xFF);    
    EEPROM.write(SRN,newSlot);
  }
  else 
  {
    int addressLastRecord = lastRecordNumber * RECORD_LENGTH;
    // record new open slot as next record of last record
    EEPROM.write(addressLastRecord + RECORD_LENGTH - 1,newSlot);

    int addressOfRecord = newSlot * RECORD_LENGTH;
    EEPROM.write(addressOfRecord,lastRecordNumber);
    for (int i=0; i<RFID_TAG_LENGTH; i++)
    {
      EEPROM.write(addressOfRecord+1+i,rfidTag[i]);
    }
    writeName(name,addressOfRecord);
    EEPROM.write(addressOfRecord+RFID_TAG_LENGTH+RECORD_NAME_LENGTH+1,0xFF);    
  }
  EEPROM.write(LRN,newSlot);
}
void retrieveRecordAtSlot(int slotNumber, Record &record)
{
  int slotAddress = slotNumber * RECORD_LENGTH;
  record.slotNumber = slotNumber;
  for (int i=0; i<RFID_TAG_LENGTH; i++)
  {
    record.rfidTag[i]=EEPROM.read(slotAddress+1+i);
  }
  for (int i=0; i < RECORD_NAME_LENGTH; i++)
  {
    record.name[i] = EEPROM.read(slotAddress + 1 + RFID_TAG_LENGTH + i);
  }
  record.nextSlotNumber = EEPROM.read(slotAddress + RECORD_LENGTH - 1);
}

void deleteRecord(uint8_t recordNumber)
{
  uint8_t firstRecordNumber = EEPROM.read(SRN);
  uint8_t lastRecordNumber = EEPROM.read(LRN);
  
  markRecordNumberFree(recordNumber, true);
  
  if (firstRecordNumber == lastRecordNumber) 
  {
    // recordNumber should equal SRN and LRN
    EEPROM.write(LRN,0xFF);
    EEPROM.write(SRN,0xFF);
    return;
  }
  
  if ((firstRecordNumber != recordNumber) & (lastRecordNumber != recordNumber))
  {
    // We are deleting a record that is not the first or last record 
    int addressOfCurrentRecord = recordNumber * RECORD_LENGTH;
    int previousRecordNumber = EEPROM.read(addressOfCurrentRecord);
    int nextRecordNumber = EEPROM.read(addressOfCurrentRecord + RECORD_LENGTH - 1);
    
    int addressPreviousRecordNumber = previousRecordNumber * RECORD_LENGTH;
    int addressNextRecordNumber = nextRecordNumber * RECORD_LENGTH;
    
    EEPROM.write(addressPreviousRecordNumber + RECORD_LENGTH - 1,nextRecordNumber);
    EEPROM.write(addressNextRecordNumber,previousRecordNumber);
    return;
  }
  
  if (firstRecordNumber == recordNumber)
  {
    // deleting the first record
    int addressOfCurrentRecord = recordNumber * RECORD_LENGTH;
    int nextRecordNumber = EEPROM.read(addressOfCurrentRecord + RECORD_LENGTH - 1);
    int addressNextRecordNumber = nextRecordNumber * RECORD_LENGTH;

    EEPROM.write(SRN,nextRecordNumber);
    EEPROM.write(addressNextRecordNumber,0xFF);
    return;  
  }
  
  // deleting the last record
  int addressOfCurrentRecord = recordNumber * RECORD_LENGTH;
  int previousRecordNumber = EEPROM.read(addressOfCurrentRecord);
  int addressPreviousRecordNumber = previousRecordNumber * RECORD_LENGTH;
  
  EEPROM.write(LRN,previousRecordNumber);
  EEPROM.write(addressPreviousRecordNumber + RECORD_LENGTH - 1,0xFF);
}
int recordCount()
{
  int count = 0;
  if (EEPROM.read(SRN) == 0xFF) return 0;
  if (EEPROM.read(SRN) == EEPROM.read(LRN)) return 1;
  
  uint8_t recordNumber = EEPROM.read(SRN);
  while (recordNumber != 0xFF) 
  {
    count++;
    recordNumber = EEPROM.read((recordNumber * RECORD_LENGTH) + RECORD_LENGTH - 1);
  }
  return count;
}

int findOpenSlot()
{
  uint8_t value;
  for (int i=0; i<5; i++)
  {
    value = EEPROM.read(STATUS + 4 - i);
    if (value != 0xFF)
    {
      for (int j=0; j<8; j++)
      {
        if (!(value & (1 << j)))
        {
          return i * 8 + j;
        }
      }
    }
  }
  return -1;
}

void markRecordNumberFree(int recordNumber, bool freeIt)
{
  int byteNumber = recordNumber / 8;
  int bitNumber = recordNumber % 8;
  uint8_t freeBits = EEPROM.read(STATUS + 4 - byteNumber);
  if (freeIt)
    freeBits &= ~(1 << bitNumber);
  else
    freeBits |= 1 << bitNumber;
  int address = STATUS + 4 - byteNumber;
  Serial.print("Writing to "); Serial.println(address,HEX);
  EEPROM.write(address,freeBits);
}

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
    int slotNumber = EEPROM.read(SRN);
    bool found = false;
    while ((slotNumber != 0xFF) & (found == false))
    {
      retrieveRecordAtSlot(slotNumber,record);
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
        int numberOfRecords = recordCount();
        unsigned char sn[2] = {SENDING_RECORD_COUNT,numberOfRecords};
        ble_write_bytes(sn,2);
        ble_do_events();
        
        int slotNumber = EEPROM.read(SRN);
        Record record;
        while (slotNumber != 0xFF)
        {
          retrieveRecordAtSlot(slotNumber,record);
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
         addRecord(uid,name);
         ble_write(SENDING_REFRESH_REQUEST);
         ble_do_events();
      }
      if (value == DELETE_RECORD)
      {
         char slotNumber = ble_read();
         deleteRecord(slotNumber);
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
