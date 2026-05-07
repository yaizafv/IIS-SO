#include "MMU.h"
#include "Buses.h"
#include "Processor.h"
#include "Simulator.h"

// The base register
int registerBase_MMU;

// The limit register
int registerLimit_MMU;

// The MAR register
int registerMAR_MMU;

// The CTRL register
int registerCTRL_MMU;

void MMU_SetCTRL(int ctrl)
{
	registerCTRL_MMU = ctrl & 0x3;

	int logicalAddress = registerMAR_MMU;
	int physicalAddress;

	switch (registerCTRL_MMU)
	{
	case CTRLREAD:
	case CTRLWRITE:

		if (Processor_PSW_BitState(EXECUTION_MODE_BIT))
		{
			if (logicalAddress < 0 || logicalAddress >= MAINMEMORYSIZE)
			{
				Processor_RaiseException(INVALIDADDRESS);
				registerCTRL_MMU = CTRL_FAIL;
			}
			else
			{
				Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
				Buses_write_ControlBus_From_To(MMU, MAINMEMORY);
				registerCTRL_MMU = CTRL_SUCCESS;
			}
		}
		else
		{
			if (logicalAddress < 0 || logicalAddress >= registerLimit_MMU)
			{
				Processor_RaiseException(INVALIDADDRESS);
				registerCTRL_MMU = CTRL_FAIL;
			}
			else
			{
				physicalAddress = registerBase_MMU + logicalAddress;
				registerMAR_MMU = physicalAddress;
				Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
				Buses_write_ControlBus_From_To(MMU, MAINMEMORY);
				registerCTRL_MMU = CTRL_SUCCESS;
			}
		}
		break;

	default:
		registerCTRL_MMU = CTRL_FAIL;
		break;
	}

	// registerCTRL_MMU return value was CTRL_SUCCESS or CTRL_FAIL
	Buses_write_ControlBus_From_To(MMU, CPU);
}

// Getter for registerCTRL_MMU
int MMU_GetCTRL()
{
	return registerCTRL_MMU;
}

// Setter for registerMAR_MMU
void MMU_SetMAR(int newMAR)
{
	registerMAR_MMU = newMAR;
}

// Getter for registerMAR_MMU
int MMU_GetMAR()
{
	return registerMAR_MMU;
}

// Setter for registerBase_MMU
void MMU_SetBase(int newBase)
{
	registerBase_MMU = newBase;
}

// Getter for registerBase_MMU
int MMU_GetBase()
{
	return registerBase_MMU;
}

// Setter for registerLimit_MMU
void MMU_SetLimit(int newLimit)
{
	registerLimit_MMU = newLimit;
}

// Getter for registerLimit_MMU
int MMU_GetLimit()
{
	return registerLimit_MMU;
}
