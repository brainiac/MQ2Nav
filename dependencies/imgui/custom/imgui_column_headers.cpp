
#include "imgui_column_headers.h"

#include <imgui.h>
#include <imgui_internal.h>

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Draw column headers
void ColumnHeaders(const char* columnsId, ColumnHeader* headers, int count, bool border)
{
	if(count<=0)
		return;

	ImGuiStyle & style = ImGui::GetStyle();
	const ImVec2 firstTextSize = ImGui::CalcTextSize(headers[0].label, NULL, true);

	ImGui::BeginChild(columnsId, ImVec2(0,firstTextSize.y + 2 * style.ItemSpacing.y), true);

	char str_id[256];
	ImFormatString(str_id, IM_ARRAYSIZE(str_id), "col_%s", columnsId);

	ImGui::Columns(count, str_id, border);

	float offset = 0.0f;

	for(int i=0; i < count; i++)
	{
		ColumnHeader & header = headers[i];

		ImGui::SetColumnOffset(i, offset);

		if(header.size < 0 || header.sizeMIN < 0)
		{
			const ImVec2 textsize = ImGui::CalcTextSize(header.label, NULL, true);
			const float colSizeX = (textsize.x + 2 * style.ItemSpacing.x);
			if(header.size    < 0)		header.size    = colSizeX;
			if(header.sizeMIN < 0)		header.sizeMIN = colSizeX;
		}

		if(header.allowResize==false)
		{
			offset += header.size;
		}
		else
		{
			if(i < (count-1))
			{
				float curOffset = offset;
				offset = ImGui::GetColumnOffset(i+1);
				header.size = offset - curOffset;
				if(header.size < header.sizeMIN)
				{
					header.size = header.sizeMIN;
					offset = curOffset + header.size;
				}
			}
		}

		ImGui::Text(header.label);
		ImGui::NextColumn();
	}

	ImGui::Columns(1);
	ImGui::EndChild();
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Synchronize with column headers
void BeginColumnHeadersSync(const char* columnsId, ColumnHeader* headers, int count, bool border)
{
	if(count<=0)
		return;

	ImGui::BeginChild(columnsId, ImVec2(0,0), true);
	ImGui::Columns(count, columnsId, border);

	ImGuiStyle & style = ImGui::GetStyle();

	float offset = 0.0f;
	for(int i=0; i < count; i++)
	{
		ColumnHeader & header = headers[i];
		ImGui::SetColumnOffset(i, offset);
		offset += header.size;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void EndColumnHeadersSync(ColumnHeader* headers, int count)
{
	if(count<=0)
		return;

	ImGui::Columns(1);
	ImGui::EndChild();
}
