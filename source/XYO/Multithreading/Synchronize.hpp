// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_SYNCHRONIZE_HPP
#define XYO_MULTITHREADING_SYNCHRONIZE_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifndef XYO_MULTITHREADING_CRITICALSECTIONLOCK_HPP
#	include <XYO/Multithreading/CriticalSectionLock.hpp>
#endif

namespace XYO::Multithreading {

	//
	// Call fn() with criticalSection entered, return its value.
	// T can be any return type: void, a reference, a type without
	// default constructor. fn is any callable (lambda, function,
	// std::function), called directly, no std::function conversion.
	//
	//   int value = Synchronize<int>::process(criticalSection, [&]() {
	//       return ++counter;
	//   });
	//
	template <typename T>
	struct Synchronize {
			template <typename F>
			static inline T process(CriticalSection &criticalSection, F &&fn) {
				CriticalSectionLock lock(criticalSection);
				return fn();
			};
	};

};

#endif
