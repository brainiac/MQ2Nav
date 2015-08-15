Prologue 
zlib general purpose compression library
version 1.2.3, July 18th, 2005 
Copyright (C) 1995-2005 Jean-loup Gailly and Mark Adler 
This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software. 
Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions: 
The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required. 
Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software. 
This notice may not be removed or altered from any source distribution. 
Jean-loup Gailly Mark Adler 
The data format used by the zlib library is described by RFCs (Request for Comments) 1950 to 1952 in the files rfc1950.txt (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format). 
Version 
#define ZLIB_VERSION "1.2.3"
#define ZLIB_VERNUM 0x1230

Introduction 
The zlib compression library provides in-memory compression and decompression functions, including integrity checks of the uncompressed data. This version of the library supports only one compression method (deflation) but other algorithms will be added later and will have the same stream interface. 
Compression can be done in a single step if the buffers are large enough (for example if an input file is mmap'ed), or can be done by repeated calls of the compression function. In the latter case, the application must provide more input and/or consume the output (providing more output space) before each call. 
The compressed data format used by default by the in-memory functions is the zlib format, which is a zlib wrapper documented in RFC 1950, wrapped around a deflate stream, which is itself documented in RFC 1951. 
The library also supports reading and writing files in gzip (.gz) format with an interface similar to that of stdio using the functions that start with "gz". The gzip format is different from the zlib format. gzip is a gzip wrapper, documented in RFC 1952, wrapped around a deflate stream. 
This library can optionally read and write gzip streams in memory as well. 
The zlib format was designed to be compact and fast for use in memory and on communications channels. The gzip format was designed for single- file compression on file systems, has a larger header than zlib to maintain directory information, and uses a different, slower check method than zlib. 
The library does not install any signal handler. The decoder checks the consistency of the compressed data, so the library should never crash even in case of corrupted input. 
