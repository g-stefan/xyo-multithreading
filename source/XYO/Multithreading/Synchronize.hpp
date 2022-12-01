// Multithreading
// Copyright (c) 2022 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2022 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_SYNCHRONIZE_HPP
#define XYO_MULTITHREADING_SYNCHRONIZE_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifdef XYO_MULTI_THREAD
namespace XYO::Multithreading {

	template <typename T>
	class Synchronize {
		public:
			static inline T process(CriticalSection &criticalSection, const std::function<T()> &fn) {
				T retV;
				criticalSection.enter();
				try {
					retV = fn();
				} catch (...) {
					criticalSection.leave();
					throw;
				}
				criticalSection.leave();
				return retV;
			};
	};

	template <>
	class Synchronize<void> {
		public:
			static inline void process(CriticalSection &criticalSection, const std::function<void()> &fn) {
				criticalSection.enter();
				try {
					fn();
				} catch (...) {
					criticalSection.leave();
					throw;
				}
				criticalSection.leave();
			};
	};
};
#endif

#ifdef XYO_SINGLE_THREAD
namespace XYO::Multithreading {

	template <typename T>
	struct Synchronize {

			static inline T process(CriticalSection &, const std::function<T()> &fn) {
				return fn();
			};
	};

	template <>
	struct Synchronize<void> {

			static inline void process(CriticalSection &, const std::function<void()> &fn) {
				fn();
			};
	};

};
#endif

#endif
