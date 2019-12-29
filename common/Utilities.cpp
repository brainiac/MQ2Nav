//
// Utilities.cpp
//

#include "Utilities.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/fonts/font_fontawesome_ttf.h>
#include <imgui/misc/fonts/font_roboto_regular_ttf.h>
#include <imgui/misc/fonts/font_materialicons_ttf.h>
#include <imgui/misc/fonts/IconsFontAwesome.h>
#include <imgui/misc/fonts/IconsMaterialDesign.h>

#include <zlib.h>
#include <cstdio>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

ImFont* ImGuiEx::DefaultFont = nullptr;
ImFont* ImGuiEx::ConsoleFont = nullptr;
ImFont* ImGuiEx::LargeIconFont = nullptr;
ImFont* ImGuiEx::LargeTextFont = nullptr;

//============================================================================

namespace ImGuiEx
{
	using namespace ImGui;

	void CenteredSeparator(float width)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return;
		auto& g = *ImGui::GetCurrentContext();
		/*
		// Commented out because it is not tested, but it should work, but it won't be centered
		ImGuiWindowFlags flags = 0;
		if ((flags & (ImGuiSeparatorFlags_Horizontal | ImGuiSeparatorFlags_Vertical)) == 0)
			flags |= (window->DC.LayoutType == ImGuiLayoutType_Horizontal) ? ImGuiSeparatorFlags_Vertical : ImGuiSeparatorFlags_Horizontal;
		IM_ASSERT(ImIsPowerOfTwo((int)(flags & (ImGuiSeparatorFlags_Horizontal | ImGuiSeparatorFlags_Vertical))));   // Check that only 1 option is selected
		if (flags & ImGuiSeparatorFlags_Vertical)
		{
			VerticalSeparator();
			return;
		}
		*/

		// Horizontal Separator
		float x1, x2;
		if (window->DC.CurrentColumns == nullptr && (width == 0))
		{
			// Span whole window
			///x1 = window->Pos.x; // This fails with SameLine(); CenteredSeparator();
			// Nah, we have to detect if we have a sameline in a different way
			x1 = window->DC.CursorPos.x;
			x2 = x1 + window->Size.x;
		}
		else
		{
			// Start at the cursor
			x1 = window->DC.CursorPos.x;
			if (width != 0) {
				x2 = x1 + width;
			}
			else
			{
				x2 = window->ClipRect.Max.x;
				// Pad right side of columns (except the last one)
				if (window->DC.CurrentColumns && (window->DC.CurrentColumns->Current < window->DC.CurrentColumns->Count - 1))
					x2 -= g.Style.ItemSpacing.x;
			}
		}
		float y1 = window->DC.CursorPos.y + int(window->DC.CurrLineSize.y / 2.0f);
		float y2 = y1 + 1.0f;

		window->DC.CursorPos.x += width; //+ g.Style.ItemSpacing.x;

		if (!window->DC.GroupStack.empty())
			x1 += window->DC.Indent.x;

		const ImRect bb(ImVec2(x1, y1), ImVec2(x2, y2));
		ItemSize(ImVec2(0.0f, 0.0f)); // NB: we don't provide our width so that it doesn't get feed back into AutoFit, we don't provide height to not alter layout.
		if (!ItemAdd(bb, NULL))
		{
			return;
		}

		window->DrawList->AddLine(bb.Min, ImVec2(bb.Max.x, bb.Min.y), GetColorU32(ImGuiCol_Border));

		/* // Commented out because LogText is hard to reach outside imgui.cpp
		if (g.LogEnabled)
		LogText(IM_NEWLINE "--------------------------------");
		*/
	}

	// Create a centered separator right after the current item.
	// Eg.: 
	// ImGui::PreSeparator(10);
	// ImGui::Text("Section VI");
	// ImGui::SameLineSeparator();
	void SameLineSeparator(float width) {
		ImGui::SameLine();
		CenteredSeparator(width);
	}

	// Create a centered separator which can be immediately followed by a item
	void PreSeparator(float width) {
		ImGuiWindow* window = GetCurrentWindow();
		if (window->DC.CurrLineSize.y == 0)
			window->DC.CurrLineSize.y = ImGui::GetTextLineHeight();
		CenteredSeparator(width);
		ImGui::SameLine();
	}

	// The value for width is arbitrary. But it looks nice.
	void TextSeparator(char* text, float pre_width)
	{
		PreSeparator(pre_width);
		Text(text);
		SameLineSeparator();
	}
}

void ImGuiEx::HelpMarker(const char* desc, float width, ImFont* tooltipFont)
{
	ImGui::TextDisabled(ICON_FA_QUESTION_CIRCLE_O);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(width);

		if (tooltipFont)
		{
			ImGui::PushFont(tooltipFont);
		}

		ImGui::TextUnformatted(desc);

		if (tooltipFont)
		{
			ImGui::PopFont();
		}

		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void ImGuiEx::ConfigureFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	// font: Roboto Regular @ 16px
	DefaultFont = io.Fonts->AddFontFromMemoryCompressedTTF(GetRobotoRegularCompressedData(),
		GetRobotoRegularCompressedSize(), 16.0);

	// font: FontAwesome
	ImFontConfig faConfig;
	faConfig.MergeMode = true;
	faConfig.GlyphMinAdvanceX = 14.0f;
	static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromMemoryCompressedTTF(GetFontAwesomeCompressedData(),
		GetFontAwesomeCompressedSize(), 13.0f, &faConfig, icon_ranges);

	// font: Material Design Icons
	ImFontConfig mdConfig;
	mdConfig.MergeMode = true;
	mdConfig.GlyphMinAdvanceX = 13.0f;
	static const ImWchar md_icon_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
	io.Fonts->AddFontFromMemoryCompressedTTF(GetMaterialIconsCompressedData(),
		GetMaterialIconsCompressedSize(), 13.0f, &mdConfig, md_icon_ranges);

	// font: Material Design Icons (Large)
	ImFontConfig mdConfig2;
	mdConfig2.GlyphMinAdvanceX = 16.0f;
	LargeIconFont = io.Fonts->AddFontFromMemoryCompressedTTF(GetMaterialIconsCompressedData(),
		GetMaterialIconsCompressedSize(), 16.0f, &mdConfig2, md_icon_ranges);

	mdConfig2.DstFont = LargeIconFont;
	mdConfig2.MergeMode = true;

	io.Fonts->AddFontFromMemoryCompressedTTF(GetRobotoRegularCompressedData(),
		GetRobotoRegularCompressedSize(), 16.0, &mdConfig2);

	// add default proggy clean font as a secondary font
	ConsoleFont = io.Fonts->AddFontDefault();

	// a large text font for headings.
	LargeTextFont = io.Fonts->AddFontFromMemoryCompressedTTF(GetRobotoRegularCompressedData(),
		GetRobotoRegularCompressedSize(), 22);
}

//----------------------------------------------------------------------------

bool CompressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data)
{
	std::vector<uint8_t> buffer;

	const size_t BUFSIZE = 128 * 1024;
	uint8_t temp_buffer[BUFSIZE];

	z_stream strm;
	strm.zalloc = 0;
	strm.zfree = 0;
	strm.next_in = reinterpret_cast<uint8_t *>(in_data);
	strm.avail_in = (uInt)in_data_size;
	strm.next_out = temp_buffer;
	strm.avail_out = BUFSIZE;

	deflateInit(&strm, Z_DEFAULT_COMPRESSION);

	while (strm.avail_in != 0)
	{
		int res = deflate(&strm, Z_NO_FLUSH);
		if (res != Z_OK)
			return false;

		if (strm.avail_out == 0)
		{
			buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
			strm.next_out = temp_buffer;
			strm.avail_out = BUFSIZE;
		}
	}

	int deflate_res = Z_OK;
	while (deflate_res == Z_OK)
	{
		if (strm.avail_out == 0)
		{
			buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
			strm.next_out = temp_buffer;
			strm.avail_out = BUFSIZE;
		}
		deflate_res = deflate(&strm, Z_FINISH);
	}

	if (deflate_res != Z_STREAM_END)
		return false;

	buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE - strm.avail_out);
	deflateEnd(&strm);

	out_data.swap(buffer);
	return true;
}

bool DecompressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data,
	size_t decompressedSize)
{
	z_stream zs; // z_stream is zlib's control structure
	memset(&zs, 0, sizeof(zs));

	if (inflateInit(&zs) != Z_OK)
		return false;

	zs.next_in = (Bytef*)in_data;
	zs.avail_in = (uInt)in_data_size;

	int ret;
	const size_t BUFSIZE = 128 * 1024;
	uint8_t temp_buffer[BUFSIZE];

	// If we didn't get a decompressedSize then do an inflate but only tally the size.
	// We do this because we really don't want to make tons of allocations as we go.
	// Once we finish, we'll run the call again with the calculated size.
	bool calculateOnly = (decompressedSize == 0);

	// Otherwise we know the size and can reserve the exact amount we need.
	std::vector<uint8_t> buffer;
	buffer.reserve(decompressedSize);

	// get the decompressed bytes blockwise using repeated calls to inflate
	do {
		zs.next_out = reinterpret_cast<Bytef*>(temp_buffer);
		zs.avail_out = BUFSIZE;

		ret = inflate(&zs, 0);

		if (!calculateOnly && buffer.size() < zs.total_out) {
			buffer.insert(buffer.end(), temp_buffer, temp_buffer + zs.total_out - buffer.size());
		}

	} while (ret == Z_OK);

	decompressedSize = zs.total_out;
	inflateEnd(&zs);

	if (ret != Z_STREAM_END) return false;

	if (calculateOnly && decompressedSize != 0)
	{
		return DecompressMemory(in_data, in_data_size, out_data, decompressedSize);
	}

	out_data.swap(buffer);
	return true;
}

//----------------------------------------------------------------------------

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

inline HINSTANCE GetComponentInstance()
{
	return (HINSTANCE)&__ImageBase;
}

std::string_view LoadResource(int resourceId)
{
	std::string_view ret;
	HINSTANCE hModule = GetComponentInstance();

	HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(resourceId), L"TEXT");
	if (hResource)
	{
		HGLOBAL hMemory = LoadResource(hModule, hResource);
		if (hMemory)
		{
			size_t size = SizeofResource(hModule, hResource);
			void* ptr = LockResource(hMemory);

			if (ptr != nullptr)
			{
				ret = { reinterpret_cast<const char*>(ptr), size };
			}
		}
	}

	return ret;
}

bool ImGuiEx::ColoredButton(const char* text, const ImVec2& size, float hue)
{
	ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(hue, 0.6f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(hue, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(hue, 0.8f, 0.8f));
	bool clicked = ImGui::Button(text, size);
	ImGui::PopStyleColor(3);
	return clicked;
}
