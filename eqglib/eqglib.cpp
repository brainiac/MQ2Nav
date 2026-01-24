
#include "pch.h"
#include "log_internal.h"

namespace eqg {

std::shared_ptr<spdlog::logger> logger;

void set_logger(std::shared_ptr<spdlog::logger> logger)
{
	eqg::logger = logger;
}

void set_logger()
{
	eqg::logger = std::make_shared<spdlog::logger>("EQG");
}

std::shared_ptr<spdlog::logger> get_logger()
{
	return eqg::logger;
}

} // namespace eqg
