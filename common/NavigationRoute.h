//
// NavigationRoute.h
//

#pragma once

#include <DetourNavMeshQuery.h>
#include <glm/glm.hpp>

#include <memory>
#include <stdexcept>

class Navigator;

struct NavigationBuffer
{
	std::unique_ptr<dtPolyRef[]> polys;
	std::unique_ptr<glm::vec3[]> points;
	std::unique_ptr<uint8_t[]> flags;
	int32_t count = 0;
	uint32_t reserved = 0;

	void Resize(uint32_t size);
	void Reset();
};

class NavigationRoute
{
	friend class Navigator;

public:
	NavigationRoute()
		: buffer(std::make_unique<NavigationBuffer>())
	{}
	NavigationRoute(std::unique_ptr<NavigationBuffer> buf)
		: buffer(std::move(buf))
	{}

	class Segment
	{
	public:
		Segment(const NavigationBuffer* buffer, uint32_t offset)
			: m_buffer(buffer)
			, m_offset(offset)
		{}

		inline uint8_t flags() const { return m_buffer->flags[m_offset]; }
		inline const glm::vec3& pos() const { return m_buffer->points[m_offset]; }
		inline dtPolyRef polyRef() const { return m_buffer->polys[m_offset]; }

	private:
		const NavigationBuffer* m_buffer;
		uint32_t m_offset = 0;
	};

	NavigationBuffer& GetBuffer() { return *buffer; }
	const NavigationBuffer& GetBuffer() const { return *buffer; }

	float GetPathTraversalDistance() const;

	using value_type = Segment;

	class iterator
	{
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = NavigationRoute::value_type;
		using difference_type = std::ptrdiff_t;
		using pointer = const value_type*;
		using reference = const value_type&;

		iterator(const NavigationRoute* path, uint32_t offset)
			: m_path(path)
			, m_offset(offset)
		{}

		bool operator==(const iterator& other) const {
			return m_path == other.m_path && m_offset == other.m_offset;
		}
		bool operator!=(const iterator& other) const {
			return !(*this == other);
		}

		iterator& operator++() {
			++m_offset;
			return *this;
		}
		iterator operator++(int amount) {
			iterator i = *this;
			m_offset += amount;
			return i;
		}
		iterator& operator+=(int amount) {
			m_offset += amount;
			return *this;
		}

		iterator& operator--() {
			--m_offset;
			return *this;
		}
		iterator operator--(int amount) {
			iterator i = *this;
			m_offset -= amount;
			return i;
		}
		iterator& operator-=(int amount) {
			m_offset -= amount;
			return *this;
		}

		difference_type operator-(const iterator& other) const {
			return m_offset - other.m_offset;
		}

		bool operator<(const iterator& other) const {
			return m_offset < other.m_offset;
		}
		bool operator<=(const iterator& other) const {
			return m_offset <= other.m_offset;
		}
		bool operator>(const iterator& other) const {
			return m_offset > other.m_offset;
		}
		bool operator>=(const iterator& other) const {
			return m_offset >= other.m_offset;
		}

		struct proxy {
			Segment s;
			proxy(Segment s_) : s(s_) {}
			pointer operator->() { return &s; }
		};

		value_type operator*() {
			return Segment(m_path->buffer.get(), m_offset);
		}

		proxy operator->() {
			return proxy{ Segment(m_path->buffer.get(), m_offset) };
		}


	private:
		const NavigationRoute* m_path;
		uint32_t m_offset = 0;
	};

	using const_iterator = iterator;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// iterator access

	iterator begin() noexcept {
		return iterator(this, 0);
	}

	const_iterator begin() const noexcept {
		return const_iterator(this, 0);
	}

	iterator end() noexcept {
		return iterator(this, buffer->count);
	}

	const_iterator end() const noexcept {
		return const_iterator(this, buffer->count);
	}

	reverse_iterator rbegin() noexcept {
		return reverse_iterator(end());
	}

	const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator(end());
	}

	reverse_iterator rend() noexcept {
		return reverse_iterator(begin());
	}

	const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator(begin());
	}

	const_iterator cbegin() const noexcept {
		return begin();
	}

	const_iterator cend() const noexcept {
		return end();
	}

	const_reverse_iterator crbegin() const noexcept {
		return rbegin();
	}

	const_reverse_iterator crend() const noexcept {
		return rend();
	}

	value_type operator[](std::size_t offset) const {
		return Segment(buffer.get(), offset);
	}

	value_type at(std::size_t offset) const {
		if (offset >= static_cast<std::size_t>(buffer->count)) {
			throw std::out_of_range("invalid NavMeshPath subscript");
		}

		return Segment(buffer.get(), offset);
	}

	value_type front() const {
		return Segment(buffer.get(), 0);
	}

	value_type back() const {
		return Segment(buffer.get(), buffer->count - 1);
	}

	bool empty() const noexcept {
		return buffer->count != 0;
	}

	std::size_t size() const noexcept {
		return static_cast<std::size_t>(buffer->count);
	}

	void swap(NavigationRoute& other) noexcept {
		using std::swap;

		swap(startPos, other.startPos);
		swap(endPos, other.endPos);
		swap(buffer, other.buffer);
	}

	void clear() noexcept {
		startPos = endPos = {};
		buffer->Reset();
	}

private:
	glm::vec3 startPos;
	glm::vec3 endPos;

	std::unique_ptr<NavigationBuffer> buffer;
};
