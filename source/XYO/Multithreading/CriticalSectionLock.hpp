// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_CRITICALSECTIONLOCK_HPP
#define XYO_MULTITHREADING_CRITICALSECTIONLOCK_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

namespace XYO::Multithreading {

	//
	// RAII lock of a CriticalSection:
	//   enter() on construction, leave() on destruction,
	//   also when leaving the scope by return or exception
	//
	//   {
	//       CriticalSectionLock lock(criticalSection);
	//       ...
	//   }
	//
	class CriticalSectionLock {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(CriticalSectionLock);

		protected:
			CriticalSection &criticalSection;

		public:
			inline CriticalSectionLock(CriticalSection &criticalSection_) : criticalSection(criticalSection_) {
				criticalSection.enter();
			};

			inline ~CriticalSectionLock() {
				criticalSection.leave();
			};
	};

};

#endif
