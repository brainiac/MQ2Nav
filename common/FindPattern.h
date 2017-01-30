//
// FindPattern.h
//

#pragma once

#ifndef _WIN64 // don't use this on 64bit platform until it gets fixed

// Find an address that matches the given pattern/mask, starting at the given dwAddress.
unsigned long FindPattern(unsigned long dwAddress, unsigned long dwLen, const unsigned char* bMask, const char* szMask);

unsigned long GetDWordAt(unsigned long address, unsigned long numBytes);

unsigned long GetFunctionAddressAt(unsigned long address, unsigned long addressOffset, unsigned long numBytes);

#endif
