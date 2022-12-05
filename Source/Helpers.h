#pragma once
#ifndef _HELPERS_H
#define _HELPERS_H
#include "SigScan.h"
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#ifdef __GNUC__
#define _MSC_VER 1929
#include "toml.hpp"
#undef _MSC_VER
#else
#include "toml.hpp"
#endif

#include <detours.h>

typedef int8_t i8, s8;
typedef int16_t i16, s16;
typedef int32_t i32, s32;
typedef int64_t i64, s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#define _CONCAT2(x, y)         x##y
#define CONCAT2(x, y)          _CONCAT (x, y)
#define INSERT_PADDING(length) uint8_t CONCAT2 (pad, __LINE__)[length]

#define ASSERT_OFFSETOF(type, field, offset) static_assert (offsetof (type, field) == offset, "offsetof assertion failed")

#define ASSERT_SIZEOF(type, size) static_assert (sizeof (type) == size, "sizeof assertion failed")

#ifdef BASE_ADDRESS
const HMODULE MODULE_HANDLE = GetModuleHandle (nullptr);

#define ASLR(address) ((size_t)MODULE_HANDLE + (size_t)address - (size_t)BASE_ADDRESS)
#endif

#define FUNCTION_PTR(returnType, callingConvention, function, location, ...)                                                                         \
	returnType (callingConvention *function) (__VA_ARGS__) = (returnType (callingConvention *) (__VA_ARGS__)) (location)

#define PROC_ADDRESS(libraryName, procName) GetProcAddress (LoadLibrary (TEXT (libraryName)), procName)

#define HOOK(returnType, callingConvention, functionName, location, ...)                                                                             \
	typedef returnType callingConvention _##functionName (__VA_ARGS__);                                                                              \
	_##functionName *original##functionName = (_##functionName *)(void *)(location);                                                                 \
	returnType callingConvention implOf##functionName (__VA_ARGS__)

#define INSTALL_HOOK(functionName)                                                                                                                   \
	{                                                                                                                                                \
		DetourTransactionBegin ();                                                                                                                   \
		DetourUpdateThread (GetCurrentThread ());                                                                                                    \
		DetourAttach ((void **)&original##functionName, (void *)implOf##functionName);                                                               \
		DetourTransactionCommit ();                                                                                                                  \
	}

#define VTABLE_HOOK(returnType, callingConvention, className, functionName, ...)                                                                     \
	typedef returnType callingConvention className##functionName (className *This, __VA_ARGS__);                                                     \
	className##functionName *original##className##functionName;                                                                                      \
	returnType callingConvention implOf##className##functionName (className *This, __VA_ARGS__)

#define INSTALL_VTABLE_HOOK(className, object, functionName, functionIndex)                                                                          \
	{                                                                                                                                                \
		void **addr                       = &(*(void ***)object)[functionIndex];                                                                     \
		original##className##functionName = (className##functionName *)*addr;                                                                        \
		DWORD oldProtect;                                                                                                                            \
		VirtualProtect (addr, sizeof (void *), PAGE_EXECUTE_READWRITE, &oldProtect);                                                                 \
		*addr = (void *)implOf##className##functionName;                                                                                             \
		VirtualProtect (addr, sizeof (void *), oldProtect, &oldProtect);                                                                             \
	}

#define WRITE_MEMORY(location, type, ...)                                                                                                            \
	{                                                                                                                                                \
		const type data[] = { __VA_ARGS__ };                                                                                                         \
		DWORD oldProtect;                                                                                                                            \
		VirtualProtect ((void *)(location), sizeof (data), PAGE_EXECUTE_READWRITE, &oldProtect);                                                     \
		memcpy ((void *)(location), data, sizeof (data));                                                                                            \
		VirtualProtect ((void *)(location), sizeof (data), oldProtect, &oldProtect);                                                                 \
	}

#define WRITE_JUMP(location, function)                                                                                                               \
	{                                                                                                                                                \
		WRITE_MEMORY (location, uint8_t, 0xE9);                                                                                                      \
		WRITE_MEMORY (location + 1, uint32_t, (uint32_t)(function) - (size_t)(location)-5);                                                          \
	}

#define WRITE_CALL(location, function)                                                                                                               \
	{                                                                                                                                                \
		WRITE_MEMORY (location, uint8_t, 0xE8);                                                                                                      \
		WRITE_MEMORY (location + 1, uint32_t, (uint32_t)(function) - (size_t)(location)-5);                                                          \
	}

#define WRITE_NOP(location, count)                                                                                                                   \
	{                                                                                                                                                \
		DWORD oldProtect;                                                                                                                            \
		VirtualProtect ((void *)(location), (size_t)(count), PAGE_EXECUTE_READWRITE, &oldProtect);                                                   \
		for (size_t i = 0; i < (size_t)(count); i++)                                                                                                 \
			*((uint8_t *)(location) + i) = 0x90;                                                                                                     \
		VirtualProtect ((void *)(location), (size_t)(count), oldProtect, &oldProtect);                                                               \
	}

#endif // _HELPERS_H
