
#pragma once


#define SafeStructAllocParse(type, var_name) if(idx + sizeof(type) > (uint32_t)buffer.size()) { return false; } \
	type *var_name = (type*)&buffer[idx]; \
	idx += sizeof(type);


#define SafeStringAllocParse(var_name) std::string var_name = &buffer[idx]; \
	if ((idx + (uint32_t)var_name.length() + 1) > (uint32_t)buffer.size()) { return false; } \
	idx += (uint32_t)var_name.length() + 1;
