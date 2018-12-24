//
// Utilities.cpp
//

#include "Utilities.h"

#include <imgui/imgui.h>
#include <imgui/misc/fonts/font_fontawesome_ttf.h>
#include <imgui/misc/fonts/font_roboto_regular_ttf.h>
#include <imgui/misc/fonts/font_materialicons_ttf.h>
#include <imgui/misc/fonts/IconsFontAwesome.h>
#include <imgui/misc/fonts/IconsMaterialDesign.h>

#include <zlib.h>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

ImFont* ImGuiEx::DefaultFont = nullptr;
ImFont* ImGuiEx::ConsoleFont = nullptr;
ImFont* ImGuiEx::LargeIconFont = nullptr;

//============================================================================

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
		GetFontAwesomeCompressedSize(), 14.0f, &faConfig, icon_ranges);

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

bool DecompressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data)
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
	std::vector<uint8_t> buffer;

	// get the decompressed bytes blockwise using repeated calls to inflate
	do {
		zs.next_out = reinterpret_cast<Bytef*>(temp_buffer);
		zs.avail_out = BUFSIZE;

		ret = inflate(&zs, 0);

		if (buffer.size() < zs.total_out) {
			buffer.insert(buffer.end(), temp_buffer, temp_buffer + zs.total_out - buffer.size());
		}

	} while (ret == Z_OK);

	inflateEnd(&zs);

	if (ret != Z_STREAM_END) return false;

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
