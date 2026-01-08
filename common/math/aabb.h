#pragma once

#include "glm/glm.hpp"


template <typename T, glm::qualifier Q = glm::defaultp>
struct taabb
{
	using value_type = T;
	using type = taabb<T, Q>;
	using vec_type = glm::vec<3, T, Q>;
	using length_type = glm::length_t;

	enum init_zero_t { init_zero };
	explicit constexpr taabb(init_zero_t) : min(0.0f), max(0.0f) {}

	enum init_invalid_t { init_invalid };
	explicit constexpr taabb(init_invalid_t) : min(std::numeric_limits<float>::max()), max(std::numeric_limits<float>::lowest()) {}

	explicit constexpr taabb(value_type v) : min(v), max(v) {}
	constexpr taabb(value_type min, value_type max) : min(min), max(max) {}
	explicit constexpr taabb(const vec_type& v) : min(v), max(v) {}
	constexpr taabb(const vec_type& min, const vec_type& max) : min(min), max(max) {}
	constexpr taabb(const taabb& other) : min(other.min), max(other.max) {}

	constexpr taabb(const vec_type& center, T radius) : taabb(init_invalid) { extend(center, radius); }

	constexpr taabb& operator=(const taabb& other) = default;
	constexpr taabb& operator=(init_zero_t) { min = vec_type{ 0.0f }; max = vec_type{ 0.0f }; return *this; }
	constexpr taabb& operator=(init_invalid_t)
	{
		min = vec_type{ std::numeric_limits<float>::max() };
		max = vec_type{ std::numeric_limits<float>::lowest() };
		return *this;
	}

	template <typename U>
	constexpr taabb& operator=(const taabb<U, Q>& other)
	{
		this->min.x = static_cast<T>(other.min.x);
		this->min.y = static_cast<T>(other.min.y);
		this->min.z = static_cast<T>(other.min.z);
		this->max.x = static_cast<T>(other.max.x);
		this->max.y = static_cast<T>(other.max.y);
		this->max.z = static_cast<T>(other.max.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator+=(U scalar)
	{
		this->min.x += static_cast<T>(scalar);
		this->min.y += static_cast<T>(scalar);
		this->min.z += static_cast<T>(scalar);
		this->max.x += static_cast<T>(scalar);
		this->max.y += static_cast<T>(scalar);
		this->max.z += static_cast<T>(scalar);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator+=(const taabb<U, Q>& other)
	{
		this->min.x += static_cast<T>(other.min.x);
		this->min.y += static_cast<T>(other.min.y);
		this->min.z += static_cast<T>(other.min.z);
		this->max.x += static_cast<T>(other.max.x);
		this->max.y += static_cast<T>(other.max.y);
		this->max.z += static_cast<T>(other.max.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator+=(const glm::vec<3, U, Q>& other)
	{
		this->min.x += static_cast<T>(other.x);
		this->min.y += static_cast<T>(other.y);
		this->min.z += static_cast<T>(other.z);
		this->max.x += static_cast<T>(other.x);
		this->max.y += static_cast<T>(other.y);
		this->max.z += static_cast<T>(other.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator-=(U scalar)
	{
		this->min.x -= static_cast<T>(scalar);
		this->min.y -= static_cast<T>(scalar);
		this->min.z -= static_cast<T>(scalar);
		this->max.x -= static_cast<T>(scalar);
		this->max.y -= static_cast<T>(scalar);
		this->max.z -= static_cast<T>(scalar);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator-=(const taabb<U, Q>& other)
	{
		this->min.x -= static_cast<T>(other.min.x);
		this->min.y -= static_cast<T>(other.min.y);
		this->min.z -= static_cast<T>(other.min.z);
		this->max.x -= static_cast<T>(other.max.x);
		this->max.y -= static_cast<T>(other.max.y);
		this->max.z -= static_cast<T>(other.max.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator-=(const glm::vec<3, U, Q>& other)
	{
		this->min.x -= static_cast<T>(other.x);
		this->min.y -= static_cast<T>(other.y);
		this->min.z -= static_cast<T>(other.z);
		this->max.x -= static_cast<T>(other.x);
		this->max.y -= static_cast<T>(other.y);
		this->max.z -= static_cast<T>(other.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator*=(U scalar)
	{
		this->min.x *= static_cast<T>(scalar);
		this->min.y *= static_cast<T>(scalar);
		this->min.z *= static_cast<T>(scalar);
		this->max.x *= static_cast<T>(scalar);
		this->max.y *= static_cast<T>(scalar);
		this->max.z *= static_cast<T>(scalar);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator*=(const taabb<U, Q>& other)
	{
		this->min.x *= static_cast<T>(other.min.x);
		this->min.y *= static_cast<T>(other.min.y);
		this->min.z *= static_cast<T>(other.min.z);
		this->max.x *= static_cast<T>(other.max.x);
		this->max.y *= static_cast<T>(other.max.y);
		this->max.z *= static_cast<T>(other.max.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator*=(const glm::vec<3, U, Q>& other)
	{
		this->min.x *= static_cast<T>(other.x);
		this->min.y *= static_cast<T>(other.y);
		this->min.z *= static_cast<T>(other.z);
		this->max.x *= static_cast<T>(other.x);
		this->max.y *= static_cast<T>(other.y);
		this->max.z *= static_cast<T>(other.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator/=(U scalar)
	{
		this->min.x /= static_cast<T>(scalar);
		this->min.y /= static_cast<T>(scalar);
		this->min.z /= static_cast<T>(scalar);
		this->max.x /= static_cast<T>(scalar);
		this->max.y /= static_cast<T>(scalar);
		this->max.z /= static_cast<T>(scalar);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator/=(const taabb<U, Q>& other)
	{
		this->min.x /= static_cast<T>(other.min.x);
		this->min.y /= static_cast<T>(other.min.y);
		this->min.z /= static_cast<T>(other.min.z);
		this->max.x /= static_cast<T>(other.max.x);
		this->max.y /= static_cast<T>(other.max.y);
		this->max.z /= static_cast<T>(other.max.z);
		return *this;
	}

	template <typename U>
	constexpr taabb& operator/=(const glm::vec<3, U, Q>& other)
	{
		this->min.x /= static_cast<T>(other.x);
		this->min.y /= static_cast<T>(other.y);
		this->min.z /= static_cast<T>(other.z);
		this->max.x /= static_cast<T>(other.x);
		this->max.y /= static_cast<T>(other.y);
		this->max.z /= static_cast<T>(other.z);
		return *this;
	}

	constexpr bool valid() const
	{
		return min.x <= max.x && min.y <= max.y && min.z <= max.z;
	}

	static constexpr length_type length() { return 6; }

	constexpr value_type& operator[](length_type i)
	{
		GLM_ASSERT_LENGTH(i, this->length());
		switch (i)
		{
		default: case 0:
			return min.x;
		case 1: return min.y;
		case 2: return min.z;
		case 3: return max.x;
		case 4: return max.y;
		case 5: return max.z;
		}
	}

	constexpr value_type const& operator[](length_type i) const
	{
		GLM_ASSERT_LENGTH(i, this->length());
		switch (i)
		{
		default: case 0:
			return min.x;
		case 1: return min.y;
		case 2: return min.z;
		case 3: return max.x;
		case 4: return max.y;
		case 5: return max.z;
		}
	}

	constexpr float width() const { return max.x - min.x; }
	constexpr float height() const { return max.y - min.y; }
	constexpr float depth() const { return max.z - min.z; }

	constexpr float volume() const { return width() * height() * depth(); }
	constexpr vec_type diagonal() const { return vec_type(width(), height(), depth()); }
	constexpr vec_type center() const { return (max + min) * 0.5f; }
	
	constexpr taabb& translate(const vec_type& v)
	{
		min += v;
		max += v;
		return *this;
	}

	constexpr taabb& stretch(value_type amount)
	{
		min -= amount;
		max += amount;
		return *this;
	}

	constexpr taabb& stretch(const vec_type& amount)
	{
		min -= amount;
		max += amount;
		return *this;
	}

	constexpr taabb& enclose(const vec_type& pos)
	{
		min = glm::min(pos, min);
		max = glm::max(pos, max);
		return *this;
	}

	constexpr taabb& unite(const taabb& other)
	{
		min = glm::min(min, other.min);
		max = glm::max(max, other.max);
		return *this;
	}

	static constexpr taabb unite(const taabb& l, const taabb& r)
	{
		return taabb(glm::min(l.min, r.min), glm::max(l.max, r.max));
	}

	constexpr bool encloses(const vec_type& vec) const
	{
		return vec >= min && vec <= max;
	}

	constexpr bool encloses(const vec_type& vec, float epsilon) const
	{
		return vec >= min - epsilon && vec <= max + epsilon;
	}

	constexpr bool encloses(const taabb& other) const
	{
		return other.min >= min && other.max <= max;
	}

	constexpr bool encloses(const taabb& other, float epsilon) const
	{
		return other.min >= min - epsilon && other.max <= max + epsilon;
	}

	constexpr bool contains(const vec_type& vec) const
	{
		return vec > min && vec < max;
	}

	constexpr bool contains(const taabb& other) const
	{
		return contains(other.min) && contains(other.max);
	}

	vec_type min;
	vec_type max;
};

template <typename T, glm::qualifier Q>
constexpr bool operator==(const taabb<T, Q>& aabb1, const taabb<T, Q>& aabb2)
{
	return aabb1.min == aabb2.min && aabb1.max == aabb2.max;
}

template <typename T, glm::qualifier Q>
constexpr bool operator!=(const taabb<T, Q>& aabb1, const taabb<T, Q>& aabb2)
{
	return aabb1.min != aabb2.min && aabb1.max != aabb2.max;
}

using aabb = taabb<float>;

namespace glm
{
	template <typename T, qualifier Q>
	GLM_FUNC_QUALIFIER T const* value_ptr(taabb<T, Q> const& v)
	{
		return &(v.min.x);
	}

	template <typename T, qualifier Q>
	GLM_FUNC_QUALIFIER T* value_ptr(taabb<T, Q>& v)
	{
		return &(v.min.x);
	}
}
