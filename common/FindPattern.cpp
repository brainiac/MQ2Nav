//
// FindPattern.cpp
//

// original code credits to:
// radioactiveman
// bunny771
// dom1n1k
// ieatacid

#include "FindPattern.h"

inline bool DataCompare(const uint8_t* pData, const uint8_t* bMask, const char* szMask)
{
	for (; *szMask; ++szMask, ++pData, ++bMask)
	{
		if (*szMask == 'x' && *pData != *bMask)
			return false;
	}
	return (*szMask) == 0;
}

uintptr_t FindPattern(uintptr_t dwAddress, uint32_t dwLen, const uint8_t* bMask, const char* szMask)
{
	if (dwAddress == 0)
		return 0;

	for (uint32_t i = 0; i < dwLen; i++)
	{
		if (DataCompare((uint8_t*)(dwAddress + i), bMask, szMask))
			return (uintptr_t)(dwAddress + i);
	}

	return 0;
}
// --------------------------------------------------------------------------------------

uint32_t GetDWordAt(uintptr_t address, uint32_t numBytes)
{
	if (address)
	{
		address += numBytes;
		return *(uint32_t*)address;
	}
	return 0;
}

uintptr_t GetFunctionAddressAt(uintptr_t address, uint32_t addressOffset, uint32_t numBytes)
{
	if (address)
	{
		uintptr_t n = *(uintptr_t*)(address + addressOffset);
		return address + n + numBytes;
	}

	return 0;
}
