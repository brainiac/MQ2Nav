
#pragma once

#include <imgui.h>

// ColumnHeader
struct ColumnHeader
{
	const char* label 	    = NULL;		// Label of the header
		  float size  	    = -1.0f;		// Negative value will calculate the size to fit label
		  float sizeMIN	    = -1.0f;		// Negative value will calculate the size to fit label
		  bool  allowResize = true;		// Allow user-resize
};

// Draw column headers
IMGUI_API void ColumnHeaders(const char* columnsId, ColumnHeader* headers, int count, bool border=true);

// Synchronize with column headers
IMGUI_API void BeginColumnHeadersSync(const char* columnsId, ColumnHeader* headers, int count, bool border=true);
IMGUI_API void EndColumnHeadersSync(ColumnHeader* headers, int count);
