// Multithreading
// Copyright (c) 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_SEMAPHORE_HPP
#define XYO_MULTITHREADING_SEMAPHORE_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifdef XYO_MULTI_THREAD

namespace XYO::Multithreading {

	class Semaphore {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Semaphore);

		protected:
			TAtomic<bool> state;

		public:
			XYO_MULTITHREADING_EXPORT Semaphore();

			XYO_MULTITHREADING_EXPORT void wait();
			XYO_MULTITHREADING_EXPORT void notify();
			XYO_MULTITHREADING_EXPORT bool peek() const;
			XYO_MULTITHREADING_EXPORT void reset();
	};

};

#endif

#endif
