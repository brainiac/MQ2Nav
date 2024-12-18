
#pragma once

#include <imgui/imgui.h>

namespace mq::imgui {

class ScopedColorStack
{
public:
	ScopedColorStack(const ScopedColorStack& other) = delete;
	ScopedColorStack& operator=(const ScopedColorStack& other) = delete;

	template <typename ValueType, typename... Others>
	ScopedColorStack(ImGuiCol firstColor, ValueType firstValue, Others&& ...others)
		: m_count((sizeof...(others) / 2) + 1)
	{
		static_assert((sizeof... (others) & 1u) == 0, "Requries a list of pairs of color vars and values");

		PushColors(firstColor, firstValue, std::forward<Others>(others)...);
	}

	~ScopedColorStack()
	{
		ImGui::PopStyleColor(m_count);
	}

private:
	template <typename ValueType, typename... Others>
	void PushColors(ImGuiCol colorVar, ValueType colorValue, Others&& ...others)
	{
		ImGui::PushStyleColor(colorVar, ImColor(colorValue).Value);

		if constexpr (sizeof...(others) != 0)
		{
			PushColors(std::forward<Others>(others)...);
		}
	}

	int m_count;
};

class ScopedStyleStack
{
public:
	ScopedStyleStack(const ScopedStyleStack& other) = delete;
	ScopedStyleStack& operator=(const ScopedStyleStack& other) = delete;

	template <typename ValueType, typename... Others>
	ScopedStyleStack(ImGuiStyleVar firstStyleVar, ValueType firstValue, Others&& ...others)
		: m_count((sizeof...(others) / 2) + 1)
	{
		static_assert((sizeof... (others) & 1u) == 0, "Requries a list of pairs of style vars and values");

		PushStyles(firstStyleVar, firstValue, std::forward<Others>(others)...);
	}

	~ScopedStyleStack()
	{
		ImGui::PopStyleVar(m_count);
	}

private:
	template <typename ValueType, typename... Others>
	void PushStyles(ImGuiStyleVar styleVar, ValueType styleValue, Others&& ...others)
	{
		ImGui::PushStyleVar(styleVar, styleValue);

		if constexpr (sizeof...(others) != 0)
		{
			PushStyles(std::forward<Others>(others)...);
		}
	}

	int m_count;
};

class ScopedFont
{
public:
	ScopedFont(const ScopedFont& other) = delete;
	ScopedFont& operator=(const ScopedFont& other) = delete;

	explicit ScopedFont(ImFont* font) { ImGui::PushFont(font); }
	~ScopedFont() { ImGui::PopFont(); }
};

class ScopedID
{
public:
	ScopedID(const ScopedID& other) = delete;
	ScopedID& operator=(const ScopedID& other) = delete;

	template <typename T> explicit ScopedID(T id) { ImGui::PushID(id); }
	~ScopedID() { ImGui::PopID(); }
};

class ScopedDisable
{
public:
	ScopedDisable(const ScopedDisable& other) = delete;
	ScopedDisable& operator=(const ScopedDisable& other) = delete;

	explicit ScopedDisable(bool disabled) { ImGui::BeginDisabled(disabled); }
	~ScopedDisable() { ImGui::EndDisabled(); }
};

} // namespace mq.imgui
