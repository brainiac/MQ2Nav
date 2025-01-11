
#pragma once

#include <spdlog/spdlog.h>

namespace eqg {

extern std::shared_ptr<spdlog::logger> logger;

#define EQG_LOG_TRACE(...) SPDLOG_LOGGER_TRACE(eqg::logger, __VA_ARGS__)
#define EQG_LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(eqg::logger, __VA_ARGS__)
#define EQG_LOG_INFO(...) SPDLOG_LOGGER_INFO(eqg::logger, __VA_ARGS__)
#define EQG_LOG_WARN(...) SPDLOG_LOGGER_WARN(eqg::logger, __VA_ARGS__)
#define EQG_LOG_ERROR(...) SPDLOG_LOGGER_ERROR(eqg::logger, __VA_ARGS__)
#define EQG_LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(eqg::logger, __VA_ARGS__)

} // namespace eqg
