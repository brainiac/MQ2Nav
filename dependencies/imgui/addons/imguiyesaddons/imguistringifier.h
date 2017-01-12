#ifndef IMGUISTRINGIFIER_H_
#define IMGUISTRINGIFIER_H_


#ifndef IMGUI_API
#include <imgui.h>	// ImVector
#endif //IMGUI_API

// Base64Encode(...) and Base64Decode(...) embed libb64 (stripped from STL <iostream> header file):
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


// USAGE
/*
// HOW DO I EMBED Base64/85 INSIDE MY SOURCE CODE ? (OTHER FORMATS WORK IN A SIMILIAR WAY)

Hp) const char* input;int inputSize;	// They contain my binary stuff (e.g. in imguihelper.h: ImGuiHelper::GetFileContent(...))
ImVector<char>& encodedText;
if (ImGui::Base64Encode(input,inputSize,encodedText,true))	// 'true' is mandatory if we want to embed the output
{
	// Write &encodedText[0] to a file (e.g. "encodedOutput.inl")
}

// To load it back inside another project: 

const char source[] =
#include "encodedOutput.inl"								// generated with Base64Encode(... , true) or similiar

ImVector<char> decodedBin;
if (ImGui::Base64Decode(source,decodedBin)) {
	// Now &decodedBin[0] should be what we need
}
// If we need the source_size we can just use:
// a) sizeof(source) if binary (= the inline file contains numbers).
// b) strlen(source) or sizeof(source)-1 if text-based (= the inline file contains a long string, so there is an implicit '\0' termination).

Also see imguibz2.h and imguihelper.h (the latter has methods to decode directly from a file path).
*/

namespace ImGui {


IMGUI_API bool Base64Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode=false,int numCharsPerLineInStringifiedMode=112);
IMGUI_API bool Base64Decode(const char* input,ImVector<char>& output);

IMGUI_API bool Base85Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode=false,int numCharsPerLineInStringifiedMode=112);
IMGUI_API bool Base85Decode(const char* input,ImVector<char>& output);

IMGUI_API bool BinaryStringify(const char* input,int inputSize,ImVector<char>& output,int numInputBytesPerLineInStringifiedMode=80);
IMGUI_API bool TextStringify(const char* input,ImVector<char>& output,int numCharsPerLineInStringifiedMode=0,int inputSize=0);

#ifdef YES_IMGUIBZ2
#ifndef BZ_DECOMPRESS_ONLY
IMGUI_API bool Bz2Base64Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode=false,int numCharsPerLineInStringifiedMode=112);
IMGUI_API bool Bz2Base85Encode(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode=false,int numCharsPerLineInStringifiedMode=112);
#endif //BZ_DECOMPRESS_ONLY
IMGUI_API bool Bz2Base64Decode(const char* input,ImVector<char>& output);
IMGUI_API bool Bz2Base85Decode(const char* input,ImVector<char>& output);
#endif //YES_IMGUIBZ2

} // namespace ImGui

#endif // IMGUISTRINGIFIER_H_

