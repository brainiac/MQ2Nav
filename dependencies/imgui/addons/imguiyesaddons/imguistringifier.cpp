#include "imguistringifier.h"



//#include <iostream>	// TO REMOVE
// LIBB64 LICENSE (libb64.sourceforge.net):
/*
Copyright-Only Dedication (based on United States law) 
or Public Domain Certification

The person or persons who have associated work with this document (the
"Dedicator" or "Certifier") hereby either (a) certifies that, to the best of
his knowledge, the work of authorship identified is in the public domain of the
country from which the work is published, or (b) hereby dedicates whatever
copyright the dedicators holds in the work of authorship identified below (the
"Work") to the public domain. A certifier, moreover, dedicates any copyright
interest he may have in the associated work, and for these purposes, is
described as a "dedicator" below.

A certifier has taken reasonable steps to verify the copyright status of this
work. Certifier recognizes that his good faith efforts may not shield him from
liability if in fact the work certified is not in the public domain.

Dedicator makes this dedication for the benefit of the public at large and to
the detriment of the Dedicator's heirs and successors. Dedicator intends this
dedication to be an overt act of relinquishment in perpetuity of all present
and future rights under copyright law, whether vested or contingent, in the
Work. Dedicator understands that such relinquishment of all rights includes
the relinquishment of all rights to enforce (by lawsuit or otherwise) those
copyrights in the Work.

Dedicator recognizes that, once placed in the public domain, the Work may be
freely reproduced, distributed, transmitted, used, modified, built upon, or
otherwise exploited by anyone for any purpose, commercial or non-commercial,
and in any way, including by methods that have not yet been invented or
conceived.
*/
namespace base64 {

// Decoder here
	extern "C"
	{
		typedef enum {step_a, step_b, step_c, step_d} base64_decodestep;
		typedef struct {base64_decodestep step;char plainchar;} base64_decodestate;

inline int base64_decode_value(char value_in)	{
	static const char decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	static const char decoding_size = sizeof(decoding);
	value_in -= 43;
	if (value_in < 0 || value_in > decoding_size) return -1;
	return decoding[(int)value_in];
}
inline void base64_init_decodestate(base64_decodestate* state_in)	{
	state_in->step = step_a;
	state_in->plainchar = 0;
}
inline int base64_decode_block(const char* code_in, const int length_in, char* plaintext_out, base64_decodestate* state_in)	{
	const char* codechar = code_in;
	char* plainchar = plaintext_out;
	char fragment;
	
	*plainchar = state_in->plainchar;
	
	switch (state_in->step)
	{
		while (1)
		{
	case step_a:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_a;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar    = (fragment & 0x03f) << 2;
	case step_b:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_b;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x030) >> 4;
			*plainchar    = (fragment & 0x00f) << 4;
	case step_c:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_c;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x03c) >> 2;
			*plainchar    = (fragment & 0x003) << 6;
	case step_d:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_d;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++   |= (fragment & 0x03f);
		}
	}
	/* control should not reach here */
	return plainchar - plaintext_out;
}
	}	// extern "C"
	struct decoder
	{
		base64_decodestate _state;
		int _buffersize;
		
		decoder(int buffersize_in = 4096) : _buffersize(buffersize_in) {}
		int decode(char value_in) {return base64_decode_value(value_in);}
		int decode(const char* code_in, const int length_in, char* plaintext_out)	{return base64_decode_block(code_in, length_in, plaintext_out, &_state);}
        /*void decode(std::istream& istream_in, std::ostream& ostream_in)
		{
			base64_init_decodestate(&_state);
			//
			const int N = _buffersize;
			char* code = new char[N];
			char* plaintext = new char[N];
			int codelength;
			int plainlength;
			
			do
			{
				istream_in.read((char*)code, N);
				codelength = istream_in.gcount();
				plainlength = decode(code, codelength, plaintext);
				ostream_in.write((const char*)plaintext, plainlength);
			}
			while (istream_in.good() && codelength > 0);
			//
			base64_init_decodestate(&_state);
			
			delete [] code;
			delete [] plaintext;
        }*/
	};

// Encoder Here
	extern "C" {
		typedef enum {step_A, step_B, step_C} base64_encodestep;
		typedef struct {base64_encodestep step;char result;int stepcount;} base64_encodestate;

const int CHARS_PER_LINE = 72;
inline void base64_init_encodestate(base64_encodestate* state_in)	{
	state_in->step = step_A;
	state_in->result = 0;
	state_in->stepcount = 0;
}
inline char base64_encode_value(char value_in)	{
	static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	if (value_in > 63) return '=';
	return encoding[(int)value_in];
}
inline int base64_encode_block(const char* plaintext_in, int length_in, char* code_out, base64_encodestate* state_in)	{
	const char* plainchar = plaintext_in;
	const char* const plaintextend = plaintext_in + length_in;
	char* codechar = code_out;
    char result = '\0';
    char fragment = '\0';
	
	result = state_in->result;
	
	switch (state_in->step)
	{
		while (1)
		{
	case step_A:
			if (plainchar == plaintextend)
			{
				state_in->result = result;
				state_in->step = step_A;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			result = (fragment & 0x0fc) >> 2;
			*codechar++ = base64_encode_value(result);
			result = (fragment & 0x003) << 4;
	case step_B:
			if (plainchar == plaintextend)
			{
				state_in->result = result;
				state_in->step = step_B;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			result |= (fragment & 0x0f0) >> 4;
			*codechar++ = base64_encode_value(result);
			result = (fragment & 0x00f) << 2;
	case step_C:
			if (plainchar == plaintextend)
			{
				state_in->result = result;
				state_in->step = step_C;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			result |= (fragment & 0x0c0) >> 6;
			*codechar++ = base64_encode_value(result);
			result  = (fragment & 0x03f) >> 0;
			*codechar++ = base64_encode_value(result);
			
			++(state_in->stepcount);
			if (state_in->stepcount == CHARS_PER_LINE/4)
			{
				*codechar++ = '\n';
				state_in->stepcount = 0;
			}
		}
	}
	/* control should not reach here */
	return codechar - code_out;
}
inline int base64_encode_blockend(char* code_out, base64_encodestate* state_in)	{
	char* codechar = code_out;
	
	switch (state_in->step)
	{
	case step_B:
		*codechar++ = base64_encode_value(state_in->result);
		*codechar++ = '=';
		*codechar++ = '=';
		break;
	case step_C:
		*codechar++ = base64_encode_value(state_in->result);
		*codechar++ = '=';
		break;
	case step_A:
		break;
	}
	*codechar++ = '\n';
	
	return codechar - code_out;
}	
	} // extern "C"
	struct encoder
	{
		base64_encodestate _state;
		int _buffersize;
		
		encoder(int buffersize_in = 4096)
		: _buffersize(buffersize_in)
		{}
		int encode(char value_in)
		{
			return base64_encode_value(value_in);
		}
		int encode(const char* code_in, const int length_in, char* plaintext_out)
		{
			return base64_encode_block(code_in, length_in, plaintext_out, &_state);
		}
		int encode_end(char* plaintext_out)
		{
			return base64_encode_blockend(plaintext_out, &_state);
		}
        /*void encode(std::istream& istream_in, std::ostream& ostream_in)
		{
			base64_init_encodestate(&_state);
			//
			const int N = _buffersize;
			char* plaintext = new char[N];
			char* code = new char[2*N];
			int plainlength;
			int codelength;
			
			do
			{
				istream_in.read(plaintext, N);
				plainlength = istream_in.gcount();
				//
				codelength = encode(plaintext, plainlength, code);
				ostream_in.write(code, codelength);
			}
			while (istream_in.good() && plainlength > 0);
			
			codelength = encode_end(code);
			ostream_in.write(code, codelength);
			//
			base64_init_encodestate(&_state);
			
			delete [] code;
			delete [] plaintext;
        }*/
	};


} // namespace base64



namespace ImGui {

namespace Stringifier {


template <typename VectorChar> static bool Base64Decode(const char* input,VectorChar& output)	{
    output.clear();if (!input) return false;
    const int N = 4096;
    base64::decoder d(N);
    base64_init_decodestate(&d._state);

    int outputStart=0,outputlength = 0;
    int codelength = strlen(input);
    const char* pIn = input;
    int stepCodeLength = 0;
    do
    {
        output.resize(outputStart+N);
        stepCodeLength = codelength>=N?N:codelength;
        outputlength = d.decode(pIn, stepCodeLength, &output[outputStart]);
        outputStart+=outputlength;
        pIn+=stepCodeLength;
        codelength-=stepCodeLength;
    }
    while (codelength>0);

    output.resize(outputStart);
    //
    base64_init_decodestate(&d._state);
    return true;
}

template <typename VectorChar> static bool Base64Encode(const char* input,int inputSize,VectorChar& output)	{
	output.clear();if (!input || inputSize==0) return false;

    const int N=4096;
    base64::encoder e(N);
    base64_init_encodestate(&e._state);

    int outputStart=0,outputlength = 0;
    int codelength = inputSize;
    const char* pIn = input;
    int stepCodeLength = 0;

    do
    {
        output.resize(outputStart+2*N);
        stepCodeLength = codelength>=N?N:codelength;
        outputlength = e.encode(pIn, stepCodeLength,&output[outputStart]);
        outputStart+=outputlength;
        pIn+=stepCodeLength;
        codelength-=stepCodeLength;
    }
    while (codelength>0);

    output.resize(outputStart+2*N);
    outputlength = e.encode_end(&output[outputStart]);
    outputStart+=outputlength;
    output.resize(outputStart);
    //
    base64_init_encodestate(&e._state);

	return true;
}



inline static unsigned int Decode85Byte(char c)   { return c >= '\\' ? c-36 : c-35; }
static void Decode85(const unsigned char* src, unsigned char* dst)	{
    while (*src)
    {
        unsigned int tmp = Decode85Byte(src[0]) + 85*(Decode85Byte(src[1]) + 85*(Decode85Byte(src[2]) + 85*(Decode85Byte(src[3]) + 85*Decode85Byte(src[4]))));
        dst[0] = ((tmp >> 0) & 0xFF); dst[1] = ((tmp >> 8) & 0xFF); dst[2] = ((tmp >> 16) & 0xFF); dst[3] = ((tmp >> 24) & 0xFF);   // We can't assume little-endianness.
        src += 5;
        dst += 4;
    }
}
template <typename VectorChar> static bool Base85Decode(const char* input,VectorChar& output)	{
	output.clear();if (!input) return false;
	const int outputSize = (((int)strlen(input) + 4) / 5) * 4;
	output.resize(outputSize);
    Decode85((const unsigned char*)input,(unsigned char*)&output[0]);
    return true;
}


inline static char Encode85Byte(unsigned int x) 	{
    x = (x % 85) + 35;
    return (x>='\\') ? x+1 : x;
}
template <typename VectorChar> static bool Base85Encode(const char* input,int inputSize,VectorChar& output,bool outputStringifiedMode,int numCharsPerLineInStringifiedMode=112)	{
    // Adapted from binary_to_compressed_c(...) inside imgui_draw.cpp
    output.clear();if (!input || inputSize==0) return false;
    output.reserve((int)((float)inputSize*1.3f));
    if (numCharsPerLineInStringifiedMode<=12) numCharsPerLineInStringifiedMode = 12;
    if (outputStringifiedMode) output.push_back('"');
    char prev_c = 0;int cnt=0;
    for (int src_i = 0; src_i < inputSize; src_i += 4)	{
        unsigned int d = *(unsigned int*)(input + src_i);
        for (unsigned int n5 = 0; n5 < 5; n5++, d /= 85)	{
            char c = Encode85Byte(d);
            if (outputStringifiedMode && c == '?' && prev_c == '?') output.push_back('\\');	// This is made a little more complicated by the fact that ??X sequences are interpreted as trigraphs by old C/C++ compilers. So we need to escape pairs of ??.
            output.push_back(c);
            prev_c = c;
        }
        cnt+=4;
        if (outputStringifiedMode && cnt>=numCharsPerLineInStringifiedMode)	{
            output.push_back('"');
            output.push_back('	');
            output.push_back('\\');
            //output.push_back(' ');
            output.push_back('\n');
            output.push_back('"');
            cnt=0;
        }
    }
    // End
    if (outputStringifiedMode) {
        output.push_back('"');
        output.push_back(';');
        output.push_back('\n');
        output.push_back('\n');
    }
    output.push_back('\0');	// End character

    return true;
}

} // namespace Stringifier



bool Base64Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)	{
    if (!stringifiedMode) return Stringifier::Base64Encode<ImVector<char> >(input,inputSize,output);
    else {
        ImVector<char> output1;
        if (!Stringifier::Base64Encode<ImVector<char> >(input,inputSize,output1)) {output.clear();return false;}
        if (output1.size()==0) {output.clear();return false;}
        if (!ImGui::TextStringify(&output1[0],output,numCharsPerLineInStringifiedMode,output1.size()-1)) {output.clear();return false;}
    }
    return true;
}
bool Base64Decode(const char* input,ImVector<char>& output)	{
    return Stringifier::Base64Decode<ImVector<char> >(input,output);
}

bool Base85Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)	{
    return Stringifier::Base85Encode<ImVector<char> >(input,inputSize,output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
bool Base85Decode(const char* input,ImVector<char>& output)	{
    return Stringifier::Base85Decode<ImVector<char> >(input,output);
}
bool BinaryStringify(const char* input, int inputSize, ImVector<char>& output, int numInputBytesPerLineInStringifiedMode) {
    output.clear();
    if (!input || inputSize<=0) return false;
    ImGuiTextBuffer b;
    b.clear();
    b.Buf.reserve(inputSize*7.5f);
    // -----------------------------------------------------------
    b.append("{%d",(int)input[0]);  // start byte
    int cnt=1;
    for (int i=1;i<inputSize;i++) {
        if (cnt++>=numInputBytesPerLineInStringifiedMode) {cnt=0;b.append("\n");}
        b.append(",%d",(int)input[i]);  // Yes, saving as unsigned char would have taken less header space,
                                        // but being consistent with all the other compressions, that take signed chars
                                        // is better.
    }
    b.append("};\n");
    //-------------------------------------------------------------
    b.Buf.swap(output);
    return true;
}
bool TextStringify(const char* input,ImVector<char>& output,int numCharsPerLineInStringifiedMode,int inputSize) {
    output.clear();if (!input) return false;
    if (inputSize<=0) inputSize=strlen(input);
    output.reserve(inputSize*1.25f);
    // --------------------------------------------------------------
    output.push_back('"');
    char c='\n';int cnt=0;bool endFile = false;
    for (int i=0;i<inputSize;i++) {
        c = input[i];
        switch (c) {
        case '\\':
            output.push_back('\\');
            output.push_back('\\');
            break;
        case '"':
            output.push_back('\\');
            output.push_back('"');
            break;
        case '\r':
        case '\n':
            //---------------------
            output.push_back('\\');
            output.push_back(c=='\n' ? 'n' : 'r');
            if (numCharsPerLineInStringifiedMode<=0)    {
                // Break at newline to ease reading:
                output.push_back('"');

                if (i==inputSize-1) {
                    endFile = true;
                    output.push_back(';');
                    output.push_back('\n');
                }
                else {
                    output.push_back('\t');
                    output.push_back('\\');
                    output.push_back('\n');
                    output.push_back('"');
                }
                cnt = 0;
                //--------------------
            }
            //--------------------
            break;
        default:
            output.push_back(c);
            if (++cnt>=numCharsPerLineInStringifiedMode && numCharsPerLineInStringifiedMode>0)    {
                //---------------------
                //output.push_back('\\');
                //output.push_back('n');
                output.push_back('"');

                if (i==inputSize-1) {
                    endFile = true;
                    output.push_back(';');
                    output.push_back('\n');
                }
                else {
                    output.push_back('\t');
                    output.push_back('\\');
                    output.push_back('\n');
                    output.push_back('"');
                }
                cnt = 0;
                //--------------------
            }
            break;

        }
    }

    if (!endFile)   {
        output.push_back('"');

        output.push_back(';');
        output.push_back('\n');
        //--------------------
    }

    output.push_back('\0');	// End character
    //-------------------------------------------------------------
    return true;
}


#ifdef YES_IMGUIBZ2
#ifndef BZ_DECOMPRESS_ONLY
bool Bz2Base64Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)    {
    output.clear();ImVector<char> output1;
    if (!ImGui::Bz2CompressFromMemory(input,inputSize,output1)) return false;
    return ImGui::Base64Encode(&output1[0],output1.size(),output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
bool Bz2Base85Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)    {
    output.clear();ImVector<char> output1;
    if (!ImGui::Bz2CompressFromMemory(input,inputSize,output1)) return false;
    return ImGui::Base85Encode(&output1[0],output1.size(),output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
#endif //BZ_DECOMPRESS_ONLY
bool Bz2Base64Decode(const char* input,ImVector<char>& output)  {
    output.clear();ImVector<char> output1;
    if (!ImGui::Base64Decode(input,output1)) return false;
    return ImGui::Bz2DecompressFromMemory(&output1[0],output1.size(),output);
}
bool Bz2Base85Decode(const char* input,ImVector<char>& output)  {
    output.clear();ImVector<char> output1;
    if (!ImGui::Base85Decode(input,output1)) return false;
    return ImGui::Bz2DecompressFromMemory(&output1[0],output1.size(),output);
}
#endif //YES_IMGUIBZ2

} // namespace ImGui




