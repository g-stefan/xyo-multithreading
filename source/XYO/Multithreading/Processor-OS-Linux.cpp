// Multithreading
// Copyright (c) 2022 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2022 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Dependency.hpp>
#include <XYO/Multithreading/Processor.hpp>

#ifdef XYO_OS_LINUX

#	include <sys/types.h>
#	include <sys/stat.h>
#	include <sys/time.h>
#	include <sys/sysinfo.h>
#	include <unistd.h>
#	include <time.h>
#	include <stdlib.h>
#	include <stdio.h>
#	include <string.h>

namespace XYO::Multithreading::Processor {

	int32_t getCount() {
		return sysconf(_SC_NPROCESSORS_ONLN);
	};

};

#endif
