#include <Servo.h>
#include <SPI.h>
#include <boards.h>
#include <ble_shield.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <avr/wdt.h>

#include <Adafruit_NFCShield_I2C.h>

#include "LinkedListEEPROM.h"

// Command from iPhone
#define LIST_RECORDS 0x01
#define ADD_RECORD 0x02
#define DELETE_RECORD 0x03
#define READ_RFID_TAG 0x04
#define ENTER_COMMAND_MODE 0x05
#define ENTER_NORMAL_OPERATION_MODE 0x06
#define OPEN_BOLT 0x08
#define CLOSE_BOLT 0x09
#define RESET_BOARD 0x0A
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

#define leaveCommandModeTime 5

#define doorClosedPin 4
#define kBoltServoPin 5
#define boltStateLED 6
#define NeopixelPin 7
#define kNumberOfPixels 16

#define sonarPin A0
#define pingEveryIteration 5000
int pingTimer;

#define kOpenPosition 10
#define kClosedPosition 170

bool boltIsOpen = false;
bool boltShouldBeOpen = false;
bool inCommandMode = true;
bool RFIDWaitingForCard = false;

Adafruit_NFCShield_I2C nfc(IRQ, RESET);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(kNumberOfPixels, NeopixelPin, NEO_GRB + NEO_KHZ800);

Servo boltServo;

uint8_t success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
unsigned int time, watchDogTimer, holdBoltOpenTimer;

LinkedListEEPROM myEEPROM;


//void retrieveRecordAtSlot(int slotNumber, Record &record);
void sendRecordToiPhone(Record record);
bool rfidTagValid(byte rfidTag[RFID_TAG_LENGTH], Record &record);

void resetArduino()
{
  wdt_enable(WDTO_15MS);
  while (1)
  {
  }
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

void handleRFIDTag(unsigned char uid[RFID_TAG_LENGTH])
{
  Record record;
  if (rfidTagValid(uid,record))
  {
    if (inCommandMode)
    {
      ble_write(SENDING_GOOD_TAG_STATUS);
      ble_do_events();
      ble_write_bytes(uid,RFID_TAG_LENGTH);
      ble_do_events();
      ble_write_bytes(record.name,RECORD_NAME_LENGTH);
      ble_do_events();
    }
    boltShouldBeOpen = true;
    colorWipe(strip.Color(0, 255, 0), 50);
    Serial.println("Tag is valid!");
  }
  else 
  {
    if (inCommandMode)
    {
      ble_write(SENDING_BAD_TAG_STATUS);
      ble_do_events();
      ble_write_bytes(uid,RFID_TAG_LENGTH);
      ble_do_events();
    }
    boltShouldBeOpen = false;
    colorWipe(strip.Color(255, 0, 0), 50);
    Serial.println("Tag is not valid!");
    holdBoltOpenTimer = 65536;
  }
}

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

void setServoOpen(bool yesNo)
{
  boltServo.attach(kBoltServoPin);
  digitalWrite(boltStateLED,yesNo);
  boltIsOpen = yesNo;
  if (yesNo)
  {
    boltServo.write(kOpenPosition);
  }
  else 
  {
    boltServo.write(kClosedPosition);
  }
  delay(500);
  boltServo.detach();  
}

void setup() {
  Serial.begin(9600);
  
  pingTimer = 0;
  
  strip.begin();
  strip.setBrightness(50);
  strip.show();
  
  nfc.begin();
  nfc.SAMConfig();

  boltServo.attach(kBoltServoPin);
  setServoOpen(false);

  pinMode(doorClosedPin,INPUT);
  pinMode(boltStateLED,OUTPUT);
  
  holdBoltOpenTimer = 0;
  
  ble_begin();
  ble_do_events();
  time = 1;
  watchDogTimer = 0;
  colorWipe(strip.Color(0, 0, 255), 50);
  delay(2000);
  setDefaultColor();
}

void enterNormalOperationMode()
{
  inCommandMode = false;
  setDefaultColor();
  ble_write(SENDING_ENTERING_NORMAL_OPERATION_MODE);
  ble_do_events();
}
void setDefaultColor()
{
    if (inCommandMode)
    {
      colorWipe(strip.Color(205, 0, 205), 50);
    }
    else 
    {
      colorWipe(strip.Color(255, 255, 0), 50);
    }
}

bool sonarDetectedCloseObject()
{
  pinMode(sonarPin, OUTPUT);
  digitalWrite(sonarPin, LOW);
  delayMicroseconds(2);
  digitalWrite(sonarPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(sonarPin, LOW);

  // The same pin is used to read the signal from the PING))): a HIGH
  // pulse whose duration is the time (in microseconds) from the sending
  // of the ping to the reception of its echo off of an object.
  pinMode(sonarPin, INPUT);
  long duration = pulseIn(sonarPin, HIGH, 2000);
  Serial.println(duration);
  return (duration > 0);
}

void checkForOpenCloseReset(char value)
{
      if (value == OPEN_BOLT)
      {
        boltShouldBeOpen = true;
        Serial.println("opening bolt from iphone");
      }
      if (value == CLOSE_BOLT)
      {
        boltShouldBeOpen = false;
        Serial.println("closing bolt from iphone");
      }
      if (value == RESET_BOARD)
      {
        resetArduino();
        Serial.println("reseting arduino commanded from iphone");
      }
}
void loop() {
  if (pingTimer >= pingEveryIteration)
  {
    pingTimer = 0;
    if (sonarDetectedCloseObject()) 
    {
      boltShouldBeOpen = true;
      colorWipe(strip.Color(0, 255, 0), 50);
    }
  }
  pingTimer++;
  
  ble_do_events();
  time++;  
  if (time == 0)
  {
    watchDogTimer++;
    if (watchDogTimer == leaveCommandModeTime)
    {
      if (inCommandMode)
        enterNormalOperationMode();
    }
  }
  
  if (boltShouldBeOpen & !boltIsOpen)
  {
    setServoOpen(true);  // Open the bolt
    holdBoltOpenTimer = 65535;
  }

  if (holdBoltOpenTimer > 0) holdBoltOpenTimer--;
  if (holdBoltOpenTimer == 0) {
    boltShouldBeOpen = false;
    setDefaultColor();
  }

  if (!boltShouldBeOpen & boltIsOpen)
  {
    if (digitalRead(doorClosedPin) == LOW)
    {
      setServoOpen(false); // Close the bolt
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
      checkForOpenCloseReset(value);
      if (value == ADD_RECORD)
      {
        Serial.println("Waiting for RFID to add to DB");
         success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,true);
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
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, true);
        nfc.PrintHex(uid, uidLength);
        handleRFIDTag(uid);
      }
    }
    else 
    {
      int value = ble_read();
      if (value == ENTER_COMMAND_MODE)
      {
        inCommandMode = true;
        setDefaultColor();

        ble_write(SENDING_ENTERING_COMMAND_MODE);
        ble_do_events();
      }
      checkForOpenCloseReset(value);
    }
  }
  else 
  {
    if (!inCommandMode) {
      if (!RFIDWaitingForCard) 
      {
        Serial.println("initiating start read...");
        nfc.startRead(PN532_MIFARE_ISO14443A, uid, &uidLength);
        RFIDWaitingForCard = true;
      }
      if (digitalRead(IRQ) == LOW) {
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, false);
        RFIDWaitingForCard = false;
        nfc.PrintHex(uid, uidLength);   
        if (ble_available() == 0)
        {
          handleRFIDTag(uid);
          delay(250);
        } 
      }
    }
  }
}
