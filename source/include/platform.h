// platform.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

// platform-specific things

#pragma once

#include "defs.h"

namespace platform
{
	#define ALLOCATE_MEMORY_FUNC						"malloc"
	#define REALLOCATE_MEMORY_FUNC						"realloc"
	#define FREE_MEMORY_FUNC							"free"


	#ifdef _WIN32
		#define WIN32_LEAN_AND_MEAN 1

		#ifndef NOMINMAX
			#define NOMINMAX
		#endif

		#include <windows.h>
		using filehandle_t = HANDLE;

		#define CRT_FDOPEN			"_fdopen"
		#define PLATFORM_NEWLINE	"\r\n"
	#else
		#include <unistd.h>
		#include <sys/stat.h>

		using filehandle_t = int;

		#define CRT_FDOPEN			"fdopen"
		#define PLATFORM_NEWLINE	"\n"
	#endif

	extern filehandle_t InvalidFileHandle;

	#define REFCOUNT_SIZE		8

	filehandle_t openFile(const char* name, int mode, int flags);
	// void deleteFile(filehandle_t fd);
	void closeFile(filehandle_t fd);

	size_t readFile(filehandle_t fd, void* buf, size_t count);
	size_t writeFile(filehandle_t fd, void* buf, size_t count);

	size_t getFileSize(const std::string& path);
	bool checkFileExists(const std::string& path);

	util::string_view readEntireFile(const std::string& path);

	std::string getFullPath(const std::string& partial);
	std::string getTemporaryFilename(const std::string& name);

	size_t getTerminalWidth();
}


