// V2-studentsCode
#ifndef OPERATINGSYSTEMBASE_H
#define OPERATINGSYSTEMBASE_H

#include "ComputerSystem.h"
#include "OperatingSystem.h"
#include "Heap.h"
#include <stdio.h>

// Prototypes of OS functions that students should not change
void OperatingSystem_LoadOperatingSystemCode(char *, int);
int OperatingSystem_ObtainAnEntryInTheProcessTable();
int OperatingSystem_ObtainProgramSize(FILE *);
int OperatingSystem_ObtainPriority(FILE *);
int OperatingSystem_LoadProgram(FILE *, int, int);
void OperatingSystem_ReadyToShutdown();
void OperatingSystem_PrepareDaemons(int);
int OperatingSystem_GetExecutingProcessID();
int OperatingSystem_IsThereANewProgram();
void OperatingSystem_PrintStatus();  // V2-studentsCode
void OperatingSystem_PrintReadyToRunQueue();  // V2-studentsCode

#define EMPTYQUEUE -1
#define NO 0
#define YES 1

// These "extern" declaration enables other source code files to gain access
// to the variable listed
extern PCB * processTable;
extern int OS_address_base;
extern int sipID;
extern char DAEMONS_PROGRAMS_FILE[];
extern char USER_PROGRAMS_FILE[];
extern char SYSTEM_IDLE_PROCESS[];
extern char OPERATING_SYSTEM_CODE_FILE[];


extern int numberOfProgramsInArrivalTimeQueue;
extern heapItem * arrivalTimeQueue;

#ifdef SLEEPINGQUEUE
extern heapItem *sleepingProcessesQueue;  // V2-studentsCode
extern int numberOfSleepingProcesses;   // V2-studentsCode
#endif

#endif
