//
// FindPattern.cpp
//

#include "FindPattern.h"

#ifndef _WIN64 // don't use this on 64bit platform until it gets fixed

// originally created by: radioactiveman/bunny771/(dom1n1k?)  ------------------------------------------
inline bool DataCompare(const unsigned char* pData, const unsigned char* bMask, const char* szMask)
{
    for (; *szMask; ++szMask, ++pData, ++bMask)
	{
		if (*szMask == 'x' && *pData != *bMask)
			return false;
	}
	return (*szMask) == 0;
}

unsigned long FindPattern(unsigned long dwAddress, unsigned long dwLen, const unsigned char* bMask, const char* szMask)
{
	for (unsigned long i = 0; i < dwLen; i++)
	{
		if (DataCompare((unsigned char*)(dwAddress + i), bMask, szMask))
			return (unsigned long)(dwAddress + i);
	}

	return 0;
}
// --------------------------------------------------------------------------------------

// ieatacid - 3/11/09
unsigned long GetDWordAt(unsigned long address, unsigned long numBytes)
{
	if (address)
	{
		address += numBytes;
		return *(unsigned long*)address;
	}
	return 0;
}

// ieatacid - 3/11/09
unsigned long GetFunctionAddressAt(unsigned long address, unsigned long addressOffset, unsigned long numBytes)
{
	if (address)
	{
		unsigned long n = *(unsigned long*)(address + addressOffset);
		return address + n + numBytes;
	}
	return 0;
}

#endif
