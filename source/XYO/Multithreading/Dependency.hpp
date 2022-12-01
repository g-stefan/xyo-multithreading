// Multithreading
// Copyright (c) 2022 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2022 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#define XYO_MULTITHREADING_DEPENDENCY_HPP

#ifndef XYO_DATASTRUCTURES_HPP
#	include <XYO/DataStructures.hpp>
#endif

// -- Export

#ifdef XYO_MULTITHREADING_INTERNAL
#	define XYO_MULTITHREADING_EXPORT XYO_LIBRARY_EXPORT
#else
#	define XYO_MULTITHREADING_EXPORT XYO_LIBRARY_IMPORT
#endif

// --

namespace XYO::Multithreading {
	using namespace XYO::ManagedMemory;
	using namespace XYO::DataStructures;
};

#endif
