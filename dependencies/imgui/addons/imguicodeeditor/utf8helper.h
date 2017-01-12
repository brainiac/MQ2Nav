/* All this code is based on the code found at: http://bjoern.hoehrmann.de/utf-8/decoder/dfa/

   This is the license of the ORIGINAL work at http://bjoern.hoehrmann.de/utf-8/decoder/dfa/:
   
   	Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE


	All the things I've added to the original code are in the public domain (but be warned that might be wrong and incorrected...)
*/

#ifndef UTF8HELPER_H_
#define UTF8HELPER_H_

//#include <sys/types.h> //uint8_t,uint32_t

class UTF8Helper {
public:
//    typedef unsigned char uint8_t;
//    typedef unsigned int uint32_t;
protected:
UTF8Helper() {}

//#define USE_RICH_FELKER_OPTIMIZATION
#ifndef USE_RICH_FELKER_OPTIMIZATION
public:

enum {
UTF8_ACCEPT=0,
UTF8_REJECT=1
};

protected:
inline static uint8_t utf8d(uint32_t byte) {
static const uint8_t _utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};
	return _utf8d[byte];
}
public:
inline static uint32_t decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d(byte);

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d(256 + *state*16 + type);
  return *state;
}

#else //USE_RICH_FELKER_OPTIMIZATION
public:

enum {
UTF8_ACCEPT=0,
UTF8_REJECT=12
};

protected:
inline static uint8_t utf8d(uint32_t byte) {
static const uint8_t _utf8d[] = {
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12, 
};	
	return _utf8d[byte];
}
public:
inline static uint32_t decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d(byte);

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d(256 + *state + type);
  return *state;
}
#undef USE_RICH_FELKER_OPTIMIZATION
#endif //USE_RICH_FELKER_OPTIMIZATION

public:



/*
USAGE
if (countCodePoints(s, &count)) printf("The string is malformed\n");
else printf("The string is %u characters long\n", count);
*/
inline int countCodePoints(uint8_t* s, size_t* count) {
    uint32_t codepoint;
    uint32_t state = 0;

    for (*count = 0; *s; ++s)
        if (!decode(&state, &codepoint, *s))
            *count += 1;

    return state != UTF8_ACCEPT;
}

// This returns the number of codepoints
inline static int CountUTF8Chars(const char* text,const char* text_end=NULL,bool* pOptionalIStringMalformedOut=NULL)   {
    uint32_t codepoint;
    uint32_t state = 0;
    int count = 0;
    const unsigned char* s = (const unsigned char*) text;
    if (!text_end) text_end = text + strlen(text);
    const unsigned char* text_end_uc = (const unsigned char*) text_end;

    for (count = 0; s!=text_end_uc; ++s)    {
        if (!decode(&state, &codepoint, *s)) ++count;
    }

    if (pOptionalIStringMalformedOut) *pOptionalIStringMalformedOut=(state != UTF8_ACCEPT);
    return count;
}

inline void printCodePoints(uint8_t* s) {
    uint32_t codepoint;
    uint32_t state = 0;

    for (; *s; ++s)
        if (!decode(&state, &codepoint, *s))
            printf("U+%04X\n", codepoint);

    if (state != UTF8_ACCEPT)
        printf("The string is not well-formed\n");
}

inline void printUTF16CodeUnits(uint8_t* s)	{
    uint32_t codepoint;
    uint32_t state = 0;
    for (; *s; ++s) {

        if (decode(&state, &codepoint, *s))
            continue;

        if (codepoint <= 0xFFFF) {
            printf("0x%04X\n", codepoint);
            continue;
        }

        // Encode code points above U+FFFF as surrogate pair.
        printf("0x%04X\n", (0xD7C0 + (codepoint >> 10)));
        printf("0x%04X\n", (0xDC00 + (codepoint & 0x3FF)));
    }
}

// This is wrong/incomplete for sure ( I still have to read the "Error recovery" section of the paper )
inline void printCodePointsWIthErrorRecovery(uint8_t* s) {
    uint32_t codepoint;
    uint32_t state = 0;

    for (uint32_t prev = 0, current = 0; *s; prev = current, ++s) {
        switch (decode(&state, &codepoint, *s))	{
        case UTF8_ACCEPT:
            // A properly encoded character has been found.
            printf("U+%04X\n", codepoint);
            break;
        case UTF8_REJECT:
            // The byte is invalid, replace it and restart.
            printf("U+FFFD (Bad UTF-8 sequence)\n");
            current = UTF8_ACCEPT;
            if (prev != UTF8_ACCEPT) s--;
            break;
        default:
            break;
        }

    }

}

int IsUTF8(uint8_t* s) {
    uint32_t codepoint, state = 0;
    while (*s) decode(&state, &codepoint, *s++);
    return state == UTF8_ACCEPT;
}


}; // class

#endif //UTF8HELPER_H_

