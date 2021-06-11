//
// pch.h
//

#pragma once

#pragma message( "Building Precompiled Header for MQ2Nav" )

#define GLM_FORCE_RADIANS

#include <imgui.h>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <ctime>
#include <functional>
#include <set>
#include <string>
#include <memory>
#include <thread>
#include <tuple>
#include <typeinfo>
#include <unordered_map>
#include <vector>


#include <DetourNavMesh.h>
#include <DetourCommon.h>
