//
// Utilities.h
//

#pragma once

// misc. utilities to help out here and there

#include <DebugDraw.h>
#include <Recast.h>
#include <RecastDump.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// OpenGL debug draw implementation.
class DebugDrawGL : public duDebugDraw
{
public:
	virtual void depthMask(bool state);
	virtual void texture(bool state);
	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f);
	virtual void vertex(const float* pos, unsigned int color);
	virtual void vertex(const float x, const float y, const float z, unsigned int color);
	virtual void vertex(const float* pos, unsigned int color, const float* uv);
	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v);
	virtual void end();
};

// stdio file implementation.
class FileIO : public duFileIO
{
public:
	FileIO();
	virtual ~FileIO();
	bool openForWrite(const char* path);
	bool openForRead(const char* path);
	virtual bool isWriting() const;
	virtual bool isReading() const;
	virtual bool write(const void* ptr, const size_t size);
	virtual bool read(void* ptr, const size_t size);

private:
	FILE* m_fp;
	int m_mode;
};

//----------------------------------------------------------------------------
// gui helpers
struct ImFont;

namespace ImGuiEx
{
	void HelpMarker(const char* text, float width = 450.0f, ImFont* tooltipFont = nullptr);

	void ConfigureFonts();

	extern ImFont* DefaultFont;
	extern ImFont* LargeIconFont;
	extern ImFont* ConsoleFont;
}

//----------------------------------------------------------------------------
// math helpers

inline uint32_t nextPow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

inline uint32_t ilog2(uint32_t v)
{
	uint32_t r = (v > 0xffff) << 4; v >>= r;
	uint32_t shift = (v > 0xff) << 3; v >>= shift; r |= shift;
	shift = (v > 0xf) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}

// Returns true if 'c' is left of line 'a'-'b'.
inline bool left(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	const float u1 = b.x - a.x;
	const float v1 = b.z - a.z;
	const float u2 = c.x - a.x;
	const float v2 = c.z - a.z;

	return u1 * v2 - v1 * u2 < 0;
}

// Returns true if 'a' is more lower-left than 'b'.
inline bool cmppt(const glm::vec3& a, const glm::vec3& b)
{
	if (a.x < b.x) return true;
	if (a.x > b.x) return false;
	if (a.z < b.z) return true;
	if (a.z > b.z) return false;
	return false;
}

inline float distSqr(const glm::vec3& v1, const glm::vec3& v2)
{
	glm::vec3 temp = v2 - v1;
	return glm::dot(temp, temp);
}

//----------------------------------------------------------------------------

bool CompressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data);
bool DecompressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data);


//----------------------------------------------------------------------------

class scope_guard
{
public:
	template<class Callable>
	scope_guard(Callable && undo_func) : f(std::forward<Callable>(undo_func)) {}
	scope_guard(scope_guard && other) : f(std::move(other.f)) {}
	~scope_guard() {
		if (f) f(); // must not throw
	}

	void dismiss() throw() {
		f = nullptr;
	}

	scope_guard(const scope_guard&) = delete;
	void operator=(const scope_guard&) = delete;

private:
	std::function<void()> f;
};



template <typename T>
using deleting_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

