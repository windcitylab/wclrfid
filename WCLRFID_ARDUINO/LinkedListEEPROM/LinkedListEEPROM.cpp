#include "Arduino.h"
#include "LinkedListEEPROM.h"
#include <EEPROM.h>

LinkedListEEPROM::LinkedListEEPROM () {
}

void LinkedListEEPROM::writeName(char name[], int atAddress)
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
void LinkedListEEPROM::addRecord(byte rfidTag[], char name[])
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
void LinkedListEEPROM::retrieveRecordAtSlot(int slotNumber, Record &record)
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
int LinkedListEEPROM::retrieveFirstSlotNumber()
{
	return EEPROM.read(SRN);
}
void LinkedListEEPROM::deleteRecord(uint8_t recordNumber)
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
int LinkedListEEPROM::recordCount()
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

int LinkedListEEPROM::findOpenSlot()
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

void LinkedListEEPROM::markRecordNumberFree(int recordNumber, bool freeIt)
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
