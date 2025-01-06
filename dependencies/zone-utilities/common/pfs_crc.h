/*
	Copyright(C) 2014 EQEmu
	
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include <cstdint>
#include <string_view>

namespace EQEmu::PFS {

class CRC
{
	enum { Polynomial = 0x04C11DB7l };

public:

	CRC() = default;
	~CRC() = default;
	CRC(const CRC&) = delete;
	CRC& operator=(const CRC&) = delete;
	
	int32_t Get(std::string_view s);
	int32_t Update(int32_t crc, std::string_view s);

private:
	int32_t Get(const uint8_t* data, int32_t length);
	int32_t Update(int32_t crc, const uint8_t* data, int32_t length);
};

} // namespace EQEmu::PFS
