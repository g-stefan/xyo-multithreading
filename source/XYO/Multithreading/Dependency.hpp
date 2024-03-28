// Multithreading
// Copyright (c) 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#define XYO_MULTITHREADING_DEPENDENCY_HPP

#ifndef XYO_DATASTRUCTURES_HPP
#	include <XYO/DataStructures.hpp>
#endif

// -- Export

#ifdef XYO_MULTITHREADING_INTERNAL
#	define XYO_MULTITHREADING_EXPORT XYO_PLATFORM_LIBRARY_EXPORT
#else
#	define XYO_MULTITHREADING_EXPORT XYO_PLATFORM_LIBRARY_IMPORT
#endif

// --

namespace XYO::Multithreading {
	using namespace XYO::ManagedMemory;
	using namespace XYO::DataStructures;

	using Platform::Multithreading::CriticalSection;
	using Platform::Multithreading::TAtomic;
	namespace Processor = Platform::Multithreading::Processor;
};

#endif
