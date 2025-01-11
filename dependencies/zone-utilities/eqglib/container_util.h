#pragma once

#include "str_util.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>

namespace eqg {

struct ci_unordered
{
private:
	struct equal
	{
		using is_transparent = void;

		template <typename T, typename U>
		bool operator()(const T& a, const U& b) const
		{
			return ci_equals(a, b);
		}
	};

	struct hasher
	{
		using is_transparent = void;

#if defined(_WIN64)
		static inline constexpr size_t FNV_offset_basis = 14695981039346656037ULL;
		static inline constexpr size_t FNV_prime = 1099511628211ULL;
#else
		static inline constexpr size_t FNV_offset_basis = 2166136261U;
		static inline constexpr size_t FNV_prime = 16777619U;
#endif

		template <typename T>
		size_t operator()(const T& a) const
		{
			// this is a re-implementation of the fnv1a hash that MSVC uses, but with tolower
			size_t hash = FNV_offset_basis;
			for (char c : a)
			{
				hash ^= static_cast<size_t>(::tolower(static_cast<unsigned char>(c)));
				hash *= FNV_prime;
			}
			return hash;
		}

		size_t operator()(const char* a) const
		{
			std::string_view sv{ a };
			return operator()(sv);
		}
	};

public:
	template <typename StringType, typename T>
	using map = std::unordered_map<StringType, T, hasher, equal>;

	template <typename StringType, typename T>
	using multimap = std::unordered_multimap<StringType, T, hasher, equal>;

	template <typename StringType>
	using set = std::unordered_set<StringType, hasher, equal>;

	template <typename StringType>
	using multiset = std::unordered_multiset<StringType, hasher, equal>;
};

struct ci_ordered
{
private:
	struct equal
	{
		using is_transparent = void;

		template <typename T>
		bool operator()(const T& a, const T& b) const
		{
			return ci_equals(a, b);
		}
	};

	struct less
	{
		using is_transparent = void;

		template <typename T>
		bool operator()(const T& a, const T&b) const
		{
			return std::string_view(a).compare(std::string_view(b)) < 0;
		}
	};

public:
	template <typename StringType, typename T>
	using map = std::map<StringType, T, less>;

	template <typename StringType, typename T>
	using multimap = std::multimap<StringType, T, less>;

	template <typename StringType>
	using set = std::set<StringType, less>;

	template <typename StringType>
	using multiset = std::multiset<StringType, less>;
};


} // namespace eqg
