#pragma once

// misc. utilities to help out here and there



// gui helpers
struct ImFont;

namespace ImGuiEx
{
	void HelpMarker(const char* text, float width = 450.0f, ImFont* tooltipFont = nullptr);

	void ConfigureFonts();

	extern ImFont* DefaultFont;
	extern ImFont* ConsoleFont;
}
