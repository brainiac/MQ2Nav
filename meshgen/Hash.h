// C++11 32bit FNV-1 and FNV-1a string hasing (Fowler–Noll–Vo hash)

#pragma once

namespace constexpr_hash32
{
	template <typename S> struct fnv_internal;
	template <typename S> struct fnv1;
	template <typename S> struct fnv1a;

	template <> struct fnv_internal<uint32_t>
	{
		constexpr static uint32_t default_offset_basis = 0x811C9DC5;
		constexpr static uint32_t prime = 0x01000193;
	};

	template <> struct fnv1<uint32_t> : public fnv_internal<uint32_t>
	{
		constexpr static inline uint32_t hash(char const* const aString, const uint32_t val = default_offset_basis)
		{
			return (aString[0] == '\0') ? val : hash(&aString[1], (val * prime) ^ uint32_t(aString[0]));
		}
	};

	template <> struct fnv1a<uint32_t> : public fnv_internal<uint32_t>
	{
		constexpr static inline uint32_t hash(char const* const aString, const uint32_t val = default_offset_basis)
		{
			return (aString[0] == '\0') ? val : hash(&aString[1], (val ^ uint32_t(aString[0])) * prime);
		}
	};
} // namespace constexpr_hash

namespace hash
{
	template <std::size_t FnvPrime, std::size_t OffsetBasis>
	struct basic_fnv_1
	{
		std::size_t operator()(const std::string_view& text) const
		{
			std::size_t hash = OffsetBasis;
			for (auto c : text)
			{
				hash *= FnvPrime;
				hash ^= c;
			}

			return hash;
		}
	};

	template <std::size_t FnvPrime, std::size_t OffsetBasis>
	struct basic_fnv_1a
	{
		std::size_t operator()(const std::string_view& text) const
		{
			std::size_t hash = OffsetBasis;
			for (auto c : text)
			{
				hash ^= c;
				hash *= FnvPrime;
			}

			return hash;
		}
	};

	// For 32 bit machines:
	//constexpr std::size_t fnv_prime = 16777619u;
	//constexpr std::size_t fnv_offset_basis = 2166136261u;

	// For 64 bit machines:
	constexpr std::size_t fnv_prime = 1099511628211u;
	constexpr std::size_t fnv_offset_basis = 14695981039346656037u;

	using fnv_1 = basic_fnv_1<fnv_prime, fnv_offset_basis>;
	using fnv_1a = basic_fnv_1a<fnv_prime, fnv_offset_basis>;
}