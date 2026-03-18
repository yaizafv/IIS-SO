#include "Simulator.h"
#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateExecutingProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRunQueue(int queueID);
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();

// The process table
PCB *processTable;

// Size of the memory occupied for the OS
int OS_MEMORY_SIZE = 32;

// Address base for OS code in this version
int OS_address_base;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation (Not assigned)
int initialPID = -1;

// Begin indes for daemons in programList
// int baseDaemonsInProgramList;

// Array that contains the identifiers of the READY processes
heapItem *readyToRunQueue[NUMBEROFQUEUES];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES];

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

int MAINMEMORYSECTIONSIZE = 60;

extern int MAINMEMORYSIZE;

int PROCESSTABLEMAXSIZE = 4;

char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

char *queueNames[NUMBEROFQUEUES] = {"HIGHPRIOUSER", "LOWPRIOUSER", "DAEMONS"};

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int programsFromFileIndex)
{

	int i, selectedProcess;

	// In this version, with original configuration of memory size (300) and number of processes (4)
	// every process occupies a 60 positions main memory chunk
	// and OS code and the system stack occupies 60 positions

	OS_address_base = MAINMEMORYSIZE - OS_MEMORY_SIZE;

	MAINMEMORYSECTIONSIZE = OS_address_base / PROCESSTABLEMAXSIZE;

	if (initialPID < 0) // if not assigned in command-line options...
		initialPID = PROCESSTABLEMAXSIZE - 1;

	// Space for the processTable
	processTable = (PCB *)malloc(PROCESSTABLEMAXSIZE * sizeof(PCB));

	// Space for the ready to run queues
	for (int i = 0; i < NUMBEROFQUEUES; i++)
	{
		readyToRunQueue[i] = Heap_create(PROCESSTABLEMAXSIZE);
		numberOfReadyToRunProcesses[i] = 0;
	}

	// Load Operating System Code
	OperatingSystem_LoadOperatingSystemCode(OPERATING_SYSTEM_CODE_FILE, OS_address_base);

	// Process table initialization (all entries are free)
	for (i = 0; i < PROCESSTABLEMAXSIZE; i++)
	{
		processTable[i].busy = 0;
		processTable[i].programListIndex = -1;
		processTable[i].initialPhysicalAddress = -1;
		processTable[i].processSize = -1;
		processTable[i].copyOfSPRegister = -1;
		processTable[i].state = -1;
		processTable[i].priority = -1;
		processTable[i].copyOfPCRegister = -1;
		processTable[i].copyOfPSWRegister = 0;
		processTable[i].programListIndex = -1;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 2);

	// Include in program list all user or system daemon processes
	OperatingSystem_PrepareDaemons(programsFromFileIndex);

	// Create and fill arrivalTimeQueue heap with user programs and daemons
	arrivalTimeQueue = Heap_create(PROGRAMSMAXNUMBER);
	ComputerSystem_FillInArrivalTimeQueue();

	ComputerSystem_PrintArrivalTimeQueue();

	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, SYSTEM_IDLE_PROCESS) != 0 && processTable[sipID].state == READY)
	{
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 99, SHUTDOWN, "FATAL ERROR: Missing SIP program!\n");
		exit(1);
	}

	// Check if at least one user process has been created
	if (numberOfNotTerminatedUserProcesses == 0)
	{
		// Simulation must finish
		OperatingSystem_ReadyToShutdown();
	}

	// At least, one process has been created
	// Select the first process that is going to use the processor
	selectedProcess = OperatingSystem_ShortTermScheduler();

	Processor_SetSSP(MAINMEMORYSIZE - 1);

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler()
{

	int createdProcessPID, i,
		numberOfSuccessfullyCreatedProcesses = 0;

	while (OperatingSystem_IsThereANewProgram() != EMPTYQUEUE)
	{
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		createdProcessPID = OperatingSystem_CreateProcess(i);
		switch (createdProcessPID)
		{
		case NOFREEENTRY:
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 50, ERROR, programList[i]->executableName);
			break;
		case PROGRAMDOESNOTEXIST:
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 51, ERROR, programList[i]->executableName, "it does not exist");
			break;
		case PROGRAMNOTVALID:
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 51, ERROR, programList[i]->executableName, "invalid priority or size");
			break;
		case TOOBIGPROCESS:
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 52, ERROR,
										programList[i]->executableName);
			break;

		default:
			// Process creation has succeeded: additional actions
			// Show message "Process [createdProcessPID] created from program [executableName]\n"
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 70, SYSPROC, createdProcessPID, programList[i]->executableName);
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(createdProcessPID);
		}
	}
	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram)
{

	int assignedPID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram = programList[indexOfExecutableProgram];

	// Obtain a process ID
	assignedPID = OperatingSystem_ObtainAnEntryInTheProcessTable();

	if (assignedPID == NOFREEENTRY)
	{
		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile = fopen(executableProgram->executableName, "r");
	if (programFile == NULL)
	{
		return PROGRAMDOESNOTEXIST;
	}
	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(programFile);

	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);

	if (processSize == PROGRAMNOTVALID || priority == PROGRAMNOTVALID)
	{
		fclose(programFile);
		return PROGRAMNOTVALID;
	}

	// Obtain enough memory space
	loadingPhysicalAddress = OperatingSystem_ObtainMainMemory(processSize, assignedPID);

	if (loadingPhysicalAddress == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}

	// Load program in the allocated memory
	OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);

	// PCB initialization
	OperatingSystem_PCBInitialization(assignedPID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);

	ComputerSystem_DebugMessage(TIMED_MESSAGE, 54, SYSPROC, assignedPID, statesNames[NEW], executableProgram->executableName);

	return assignedPID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID)
{

	if (processSize > MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;

	return PID * MAINMEMORYSECTIONSIZE;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex)
{

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].copyOfSPRegister = initialPhysicalAddress + processSize;
	processTable[PID].state = NEW;
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
	// Daemons run in protected mode and MMU use real address

	processTable[PID].copyOfAccumulator = 0;
	processTable[PID].copyOfRegisterA = 0;
	processTable[PID].copyOfRegisterB = 0;

	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].queueID = DAEMONSQUEUE;
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
	}
	else
	{
		processTable[PID].queueID = (processSize < 30) ? HIGHPRIOUSERPROCQUEUE : LOWPRIOUSERPROCQUEUE;
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
	}
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID)
{

	int processQueueID = processTable[PID].queueID;

	if (Heap_add(PID, readyToRunQueue[processQueueID], QUEUE_PRIORITY, &(numberOfReadyToRunProcesses[processQueueID])) >= 0)
	{
		processTable[PID].state = READY;
	}
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 53, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[NEW], statesNames[READY]);
	OperatingSystem_PrintReadyToRunQueue(processQueueID);
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler()
{

	for (int q = 0; q < NUMBEROFQUEUES; ++q)
	{
		if (numberOfReadyToRunProcesses[q] > 0)
		{
			return OperatingSystem_ExtractFromReadyToRunQueue(q);
		}
	}
	return NOPROCESS;
}

// Return PID of process with the highest priority in the READY queue
int OperatingSystem_ExtractFromReadyToRunQueue(int queueID)
{

	int selectedProcess = NOPROCESS;

	selectedProcess = Heap_poll(readyToRunQueue[queueID], QUEUE_PRIORITY, &(numberOfReadyToRunProcesses[queueID]));

	// Return highest priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID)
{

	// The process identified by PID becomes the current executing process
	executingProcessID = PID;

	enum ProcessStates previousState = processTable[PID].state;

	// Change the process' state
	processTable[PID].state = EXECUTING;

	ComputerSystem_DebugMessage(TIMED_MESSAGE, 53, SYSPROC,
								PID,
								programList[processTable[PID].programListIndex]->executableName,
								statesNames[previousState],
								statesNames[EXECUTING]);
	OperatingSystem_PrintReadyToRunQueue(processTable[PID].queueID);
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID)
{

	// New values for the CPU registers are obtained from the PCB
	Processor_PushInSystemStack(processTable[PID].copyOfPCRegister);
	Processor_PushInSystemStack(processTable[PID].copyOfPSWRegister);
	Processor_SetRegisterSP(processTable[PID].copyOfSPRegister);

	Processor_SetAccumulator(processTable[PID].copyOfAccumulator);
	Processor_SetRegisterA(processTable[PID].copyOfRegisterA);
	Processor_SetRegisterB(processTable[PID].copyOfRegisterB);

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}

// Function invoked when the executing process leaves the CPU
void OperatingSystem_PreemptRunningProcess()
{
	enum ProcessStates previousState = processTable[executingProcessID].state;
	processTable[executingProcessID].state = READY;

	ComputerSystem_DebugMessage(TIMED_MESSAGE, 53, SYSPROC,
								executingProcessID,
								programList[processTable[executingProcessID].programListIndex]->executableName,
								statesNames[previousState],
								statesNames[READY]);

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID = NOPROCESS;
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID)
{

	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_PopFromSystemStack();

	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_PopFromSystemStack();

	// Save RegisterSP
	processTable[PID].copyOfSPRegister = Processor_GetRegisterSP();

	processTable[PID].copyOfAccumulator = Processor_GetAccumulator();
	processTable[PID].copyOfRegisterA = Processor_GetRegisterA();
	processTable[PID].copyOfRegisterB = Processor_GetRegisterB();
}

// Exception management routine
void OperatingSystem_HandleException()
{

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 54, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

	OperatingSystem_TerminateExecutingProcess();
}

// All tasks regarding the removal of the executing process
void OperatingSystem_TerminateExecutingProcess()
{

	enum ProcessStates previousState = processTable[executingProcessID].state;
	processTable[executingProcessID].state = EXIT;

	ComputerSystem_DebugMessage(TIMED_MESSAGE, 53, SYSPROC,
								executingProcessID,
								programList[processTable[executingProcessID].programListIndex]->executableName,
								statesNames[previousState],
								statesNames[EXIT]);

	if (executingProcessID == sipID)
	{
		// finishing sipID, change PC to address of OS HALT instruction
		Processor_SetSSP(MAINMEMORYSIZE - 1);
		Processor_PushInSystemStack(OS_address_base + 1);
		Processor_PushInSystemStack(Processor_GetPSW());
		executingProcessID = NOPROCESS;
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 99, SHUTDOWN, "The system will shut down now...\n");
		return; // Don't dispatch any process
	}

	Processor_SetSSP(Processor_GetSSP() + 2); // unstack PC and PSW stacked

	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;

	if (numberOfNotTerminatedUserProcesses == 0)
	{
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	int selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall()
{

	int systemCallID;

	// Register C contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterC();

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECINFO:
		// Show message: "Process [executingProcessID] is using the CPU ...\n"
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, Processor_GetRegisterA(), Processor_GetRegisterB(), processTable[executingProcessID].copyOfPCRegister);
		break;

	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateExecutingProcess();
		break;

	case SYSCALL_YIELD:
	{
		int currentPID = executingProcessID;
		int q = processTable[currentPID].queueID;

		if (numberOfReadyToRunProcesses[q] > 0)
		{
			int topPID = Heap_getFirst(readyToRunQueue[q], numberOfReadyToRunProcesses[q]);

			if (topPID != NOPROCESS &&
				processTable[topPID].priority == processTable[currentPID].priority)
			{
				ComputerSystem_DebugMessage(
					TIMED_MESSAGE, 55, SHORTTERMSCHEDULE,
					currentPID,
					programList[processTable[currentPID].programListIndex]->executableName,
					topPID,
					programList[processTable[topPID].programListIndex]->executableName);

				OperatingSystem_PreemptRunningProcess();
				int selected = OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(selected);
			}
			else
			{
				ComputerSystem_DebugMessage(
					TIMED_MESSAGE, 56, SHORTTERMSCHEDULE,
					currentPID,
					programList[processTable[currentPID].programListIndex]->executableName);
			}
		}
		else
		{
			ComputerSystem_DebugMessage(
				TIMED_MESSAGE, 56, SHORTTERMSCHEDULE,
				executingProcessID,
				programList[processTable[executingProcessID].programListIndex]->executableName);
		}
		break;
	}
	}
}

//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint)
{
	switch (entryPoint)
	{
	case SYSCALL_BIT: // SYSCALL_BIT=2
		OperatingSystem_HandleSystemCall();
		break;
	case EXCEPTION_BIT: // EXCEPTION_BIT=6
		OperatingSystem_HandleException();
		break;
	}
}

void OperatingSystem_PrintReadyToRunQueue() {
    ComputerSystem_DebugMessage(TIMED_MESSAGE, 108, SHORTTERMSCHEDULE);
    for (int q = 0; q < NUMBEROFQUEUES; q++) {
        ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 109, SHORTTERMSCHEDULE,
                                    queueNames[q]);
        if (numberOfReadyToRunProcesses[q] == 0) {
            ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 111, SHORTTERMSCHEDULE);
        } else {
            Heap_print(readyToRunQueue[q], QUEUE_PRIORITY, numberOfReadyToRunProcesses[q]);
            ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 111, SHORTTERMSCHEDULE);
        }
    }
}



