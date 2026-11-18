#pragma once
#include <stdlib.h>
#if defined(_WIN32) && !defined(__MINGW32__)
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

typedef unsigned long DWORD;
#else
#include <stdint.h>
typedef unsigned long DWORD;
#define PtrToUlong(ptr) ((unsigned long)(reinterpret_cast<uintptr_t>(ptr)))
#endif