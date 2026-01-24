#pragma once

#include "eqg_loader.h"
#include "eqg_resource_manager.h"
#include "wld_loader.h"

namespace spdlog
{
	class logger;
}

namespace eqg {

void set_logger(std::shared_ptr<spdlog::logger> logger);
void set_logger();

std::shared_ptr<spdlog::logger> get_logger();

} // namespace eqg
