#include "Arduino.h"

#ifndef LinkedListEEPROM_h
#define LinkedListEEPROM_h

#define RECORD_NAME_LENGTH 20
#define RFID_TAG_LENGTH 4
#define RECORD_LENGTH 26
#define LRN 0x3FA // last record number
#define SRN 0x3F9 // starting record number
#define STATUS 0x3FB

typedef struct Record {
  int slotNumber;
  byte rfidTag[RFID_TAG_LENGTH];
  unsigned char name[RECORD_NAME_LENGTH];
  int nextSlotNumber;
} Record;

class LinkedListEEPROM
{
public:
	LinkedListEEPROM();
	void writeName(char name[], int atAddress);
	void addRecord(byte rfidTag[], char name[]);
	void retrieveRecordAtSlot(int slotNumber, Record &record);
	void deleteRecord(uint8_t recordNumber);
	int recordCount();
	int findOpenSlot();
	void markRecordNumberFree(int recordNumber, bool freeIt);
	int retrieveFirstSlotNumber();
};

#endif