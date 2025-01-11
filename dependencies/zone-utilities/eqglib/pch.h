#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#if defined(_DEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>
