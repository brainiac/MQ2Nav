
#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <span>
#include <type_traits>

namespace EQEmu {

class OverreadException : public std::exception
{
public:
	OverreadException(uint32_t pos, uint32_t size, const char* msg)
		: std::exception(msg), pos_(pos), size_(size) {}

	uint32_t pos() const { return pos_; }
	uint32_t size() const { return size_; }

private:
	uint32_t pos_;
	uint32_t size_;
};

class ReadBuffer
{
public:
	ReadBuffer(const uint8_t* data, size_t sz)
		: buffer_(data), size_(sz)
	{
	}

	ReadBuffer(const char* data, size_t sz)
		: buffer_(reinterpret_cast<const uint8_t*>(data)), size_(sz)
	{
	}

	ReadBuffer(const std::unique_ptr<uint8_t[]>& data, size_t sz)
		: buffer_(data.get()), size_(sz)
	{
	}

	ReadBuffer(const std::unique_ptr<char[]>& data, size_t sz)
		: buffer_(reinterpret_cast<const uint8_t*>(data.get())), size_(sz)
	{
	}

	ReadBuffer(std::span<uint8_t> span)
		: buffer_(span.data()), size_(span.size())
	{
	}

	ReadBuffer(std::span<char> span)
		: buffer_(reinterpret_cast<const uint8_t*>(span.data())), size_(span.size())
	{
	}

	ReadBuffer(std::span<const uint8_t> span)
		: buffer_(span.data()), size_(span.size())
	{
	}

	ReadBuffer(std::span<const char> span)
		: buffer_(reinterpret_cast<const uint8_t*>(span.data())), size_(span.size())
	{
	}

	const uint8_t& operator[](size_t pos) const { return buffer_[pos]; }
	const uint8_t* data() const { return buffer_ + pos_; }

	operator bool() { return size_ - pos_ > 0; }
	operator bool() const { return size_ - pos_ > 0; }

	bool empty() { return size_ == 0; }
	size_t size() const { return size_; }
	size_t remaining() const { return size_ - pos_; }

	template <typename T>
	bool read(T& data)
	{
		static_assert(std::is_standard_layout<T>::value,
			"ReadBuffer::read<T>() only works on pod and string types.");

		return read(reinterpret_cast<uint8_t*>(&data), sizeof(T));
	}

	bool read(std::string& str)
	{
		std::string temp(reinterpret_cast<const char*>(buffer_) + pos_);
		if (temp.length() + pos_ > size_)
			return false;

		pos_ += temp.length() + 1;
		str = std::move(temp);
		return true;
	}

	template <typename T>
	bool read(T* buf, size_t len)
	{
		static_assert(std::is_standard_layout<T>::value,
			"ReadBuffer::read<T>() only works on pod and string types.");

		size_t item_size = sizeof(T) * len;
		if (pos_ + item_size > size_)
			return false;

		memcpy(buf, buffer_ + pos_, item_size);
		pos_ += item_size;
		return true;
	}

	bool read(std::span<const uint8_t>& span, size_t len)
	{
		if (pos_ + len > size_)
			return false;

		span = std::span<const uint8_t>(buffer_ + pos_, len);
		pos_ += len;
		return true;
	}

	template <typename T>
	T* read_ptr()
	{
		size_t item_size = sizeof(T);

		if (pos_ + item_size > size_)
			return nullptr;

		T* ptr = (T*)(buffer_ + pos_);
		pos_ += item_size;
		return ptr;
	}

	char* read_cstr()
	{
		size_t len = 0;
		while (pos_ + len < size_ && buffer_[pos_ + len] != 0)
			++len;
		if (pos_ + len > size_ || buffer_[pos_ + len] != 0)
			return nullptr;
		char* str = (char*)(buffer_ + pos_);
		pos_ += len;
		return str;
	}

	template <typename T>
	T* read_array(size_t count)
	{
		size_t len = sizeof(T) * count;
		if (pos_ + len > size_)
			return nullptr;

		T* arr = (T*)(buffer_ + pos_);
		pos_ += len;
		return arr;
	}

	template <typename T>
	T read()
	{
		T data;
		if (!read(data))
			throw OverreadException(static_cast<uint32_t>(pos_), sizeof(T), "ReadBuffer::read<T>() overread.");
		return data;
	}

	size_t pos() const { return pos_; }
	void pos(size_t rp) { pos_ = rp; }

	bool skip(size_t skip)
	{
		if (pos_ + skip > size_)
			return false;
		pos_ += skip;
		return true;
	}

	template <typename T>
	bool skip(uint32_t count = 1)
	{
		static_assert(std::is_standard_layout<T>::value,
			"ReadBuffer::skip<T>() only works on pod and string types.");
	
		return skip(sizeof(T) * count);
	}

	std::span<const uint8_t> span() const
	{
		return std::span<const uint8_t>(buffer_ + pos_, size_ - pos_);
	}

	std::span<const uint8_t> span(size_t len) const
	{
		return std::span<const uint8_t>(buffer_ + pos_, len);
	}

private:
	const uint8_t* buffer_ = nullptr;
	size_t size_ = 0;
	size_t pos_ = 0;
};

} // namespace EQEmu
