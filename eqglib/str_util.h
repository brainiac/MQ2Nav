#pragma once

// String utilities
#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace eqg {

inline void to_lower(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

inline std::string to_lower_copy(std::string_view str)
{
	std::string tmp(str);
	to_lower(tmp);
	return tmp;
}

inline void to_upper(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}

inline std::string to_upper_copy(std::string_view str)
{
	std::string tmp(str);
	to_upper(tmp);
	return tmp;
}


// trim from start (in place)
inline void ltrim(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		[](int ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
		[](int ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string& s)
{
	ltrim(s);
	rtrim(s);
}

// trim from start (copying)
inline std::string ltrim_copy(std::string s)
{
	ltrim(s);
	return s;
}

// trim from end (copying)
inline std::string rtrim_copy(std::string s)
{
	rtrim(s);
	return s;
}

// trim from both ends (copying)
inline std::string trim_copy(std::string s)
{
	trim(s);
	return s;
}

[[nodiscard]]
inline std::string_view ltrim(std::string_view s)
{
	s.remove_prefix(std::min(s.find_first_not_of(" \t\r\n"), s.size()));
	return s;
}

[[nodiscard]]
inline std::string_view rtrim(std::string_view s)
{
	s.remove_suffix(std::min(s.size() - s.find_last_not_of(" \t\r\n") - 1, s.size()));
	return s;
}

[[nodiscard]]
inline std::string_view trim(std::string_view s)
{
	return rtrim(ltrim(s));
}

inline std::vector<std::string_view> split_view(std::string_view s, char delim, bool skipAdjacent = false)
{
	std::vector<std::string_view> elems;

	size_t start_idx = 0;

	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == delim)
		{
			std::string_view sv{ s.data() + start_idx, i - start_idx };
			if (!sv.empty() || !skipAdjacent)
			{
				elems.push_back(sv);
			}
			start_idx = i + 1;
		}
		else if (i == s.size() - 1)
		{
			// get the last element, which needs to include the ith character since it's not a delimiter
			std::string_view sv{ s.data() + start_idx, s.size() - start_idx };
			if (!sv.empty() || !skipAdjacent)
			{
				elems.emplace_back(sv);
			}
		}
	}

	return elems;
}

// allocates a string from a string_view, replaces all occurrences of each
// entry in `to_replace` and returns this string
inline std::string replace(std::string_view str, std::vector<std::pair<std::string_view, std::string_view>> to_replace)
{
	std::string s(str);
	for (auto r : to_replace)
	{
		std::string::size_type p = 0;
		while ((p = s.find(r.first, p)) != std::string::npos)
		{
			s.replace(p, r.first.length(), r.second);
			p += r.second.length();
		}
	}

	return s;
}

inline std::string replace(std::string_view str, std::string_view search, std::string_view replacement)
{
	std::string s(str);

	std::string::size_type p = 0;
	while ((p = s.find(search, p)) != std::string::npos)
	{
		s.replace(p, search.length(), replacement);
		p += replacement.length();
	}

	return s;
}


inline int find_substr(std::string_view haystack, std::string_view needle)
{
	auto iter = std::search(std::begin(haystack), std::end(haystack),
		std::begin(needle), std::end(needle));
	if (iter == std::end(haystack)) return -1;
	return static_cast<int>(iter - std::begin(haystack));
}

struct ci_less
{
	struct nocase_compare
	{
		bool operator() (unsigned char c1, unsigned char c2) const noexcept
		{
			if (c1 == c2)
				return false;
			return ::tolower(c1) < ::tolower(c2);
		}
	};

	struct nocase_equals
	{
		bool operator() (unsigned char c1, unsigned char c2) const noexcept
		{
			if (c1 == c2)
				return true;

			return ::tolower(c1) == ::tolower(c2);
		}
	};

	struct nocase_equals_w
	{
		bool operator() (wchar_t c1, wchar_t c2) const noexcept
		{
			if (c1 == c2)
				return true;

			return ::towlower(c1) == ::towlower(c2);
		}
	};

	bool operator()(std::string_view s1, std::string_view s2) const noexcept
	{
		return std::lexicographical_compare(
			s1.begin(), s1.end(),
			s2.begin(), s2.end(),
			nocase_compare());
	}

	using is_transparent = void;
};

inline int ci_find_substr(std::string_view haystack, std::string_view needle)
{
	auto iter = std::search(std::begin(haystack), std::end(haystack),
		std::begin(needle), std::end(needle), ci_less::nocase_equals());
	if (iter == std::end(haystack)) return -1;
	return static_cast<int>(iter - std::begin(haystack));
}

inline int ci_find_substr_w(std::wstring_view haystack, std::wstring_view needle)
{
	auto iter = std::search(std::begin(haystack), std::end(haystack),
		std::begin(needle), std::end(needle), ci_less::nocase_equals_w());
	if (iter == std::end(haystack)) return -1;
	return static_cast<int>(iter - std::begin(haystack));
}

inline bool ci_equals(std::string_view sv1, std::string_view sv2)
{
	return sv1.size() == sv2.size()
		&& std::equal(sv1.begin(), sv1.end(), sv2.begin(), ci_less::nocase_equals());
}

inline bool ci_equals(std::wstring_view sv1, std::wstring_view sv2)
{
	return sv1.size() == sv2.size()
		&& std::equal(sv1.begin(), sv1.end(), sv2.begin(), ci_less::nocase_equals_w());
}

inline bool ci_equals(std::string_view haystack, std::string_view needle, bool isExact)
{
	if (isExact)
		return ci_equals(haystack, needle);

	return ci_find_substr(haystack, needle) != -1;
}

inline bool string_equals(std::string_view sv1, std::string_view sv2)
{
	return sv1.size() == sv2.size()
		&& std::equal(sv1.begin(), sv1.end(), sv2.begin());
}

inline bool starts_with(std::string_view a, std::string_view b)
{
	if (a.length() < b.length())
		return false;

	return a.substr(0, b.length()).compare(b) == 0;
}

inline bool ci_starts_with(std::string_view a, std::string_view b)
{
	if (a.length() < b.length())
		return false;

	return ci_equals(a.substr(0, b.length()), b);
}

inline bool ends_with(std::string_view a, std::string_view b)
{
	if (a.length() < b.length())
		return false;

	return a.substr(a.length() - b.length()).compare(b) == 0;
}

inline bool ci_ends_with(std::string_view a, std::string_view b)
{
	if (a.length() < b.length())
		return false;

	return ci_equals(a.substr(a.length() - b.length()), b);
}

inline int ci_char_compare(char a, char b)
{
	return std::tolower(static_cast<unsigned char>(a)) - std::tolower(static_cast<unsigned char>(b));
}

inline int ci_string_compare(std::string_view s1, std::string_view s2)
{
	size_t size = std::min(s1.size(), s2.size());

	for (size_t i = 0; i < size; ++i)
	{
		if (int result = ci_char_compare(s1[i], s2[i]); result != 0)
			return result < 0 ? -1 : 1;
	}

	if (s1.size() < s2.size())
		return -1;
	if (s1.size() > s2.size())
		return 1;
	return 0;
}

inline float str_to_float(const std::string_view svString, float fReturnOnFail)
{
	auto trimmed = trim(svString);
	std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), fReturnOnFail);
	// Could error check here, but failures don't modify the value and we're not returning meaningful errors.
	return fReturnOnFail;
}

inline int str_to_int(const std::string_view svString, int iReturnOnFail)
{
	auto trimmed = trim(svString);
	auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), iReturnOnFail);
	// Could error check here, but failures don't modify the value and we're not returning meaningful errors.
	return iReturnOnFail;
}

inline bool str_to_bool(const std::string_view svString, const bool defaultValue)
{
	const auto trimmed = trim(svString);

	if (trimmed.empty())
		return false;

	switch (trimmed[0])
	{
	case 'T':
	case 't': // true
		if (trimmed.length() == 4 && ci_equals(trimmed, "true"))
			return true;
		return defaultValue;

	case 'F':
	case 'f': // false
		if (trimmed.length() == 5 && ci_equals(trimmed, "false"))
			return false;
		return defaultValue;

	case 'O':
	case 'o': // on
		if (trimmed.length() == 2 && (trimmed[1] == 'n' || trimmed[1] == 'N'))
			return true;
		return defaultValue;

	case 'Y':
	case 'y': // yes
		if (trimmed.length() == 3 && (trimmed[1] == 'e' || trimmed[1] == 'E')
			&& (trimmed[2] == 's' || trimmed[2] == 'S'))
			return true;
		return defaultValue;

	case 'N':
	case 'n': // no
		if (trimmed.length() == 2 && (trimmed[1] == 'o' || trimmed[1] == 'O'))
			return false;
		return defaultValue;

	case '0':
		if (trimmed.length() == 1)
			return false;

		[[fallthrough]];
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if (trimmed.length() == 1)
			return trimmed[0] != '0';

		return static_cast<bool>(str_to_int(trimmed, static_cast<int>(defaultValue)));

	default:
		return defaultValue;
	}
}

} // namespace eqg
