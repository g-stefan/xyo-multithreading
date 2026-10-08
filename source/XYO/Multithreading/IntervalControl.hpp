// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_INTERVALCONTROL_HPP
#define XYO_MULTITHREADING_INTERVALCONTROL_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	//
	// Stops Thread::setInterval / Thread::setIntervalActionFirst right away:
	// clear() wakes the waiting interval thread, no polling.
	//
	//   IntervalControl control;      // declared first, outlives the thread
	//   TPointer<Thread> thread = Thread::setInterval(control, 100, procedure, this_);
	//   ...
	//   control.clear();              // no call after clear
	//   thread->join();
	//
	// - clear() from any thread, one control can stop several intervals
	// - isCleared() from any thread
	// - reset() to use it again, when no interval uses it
	// - waitFor() used by the interval thread: waits milliSeconds,
	//   returns false right away when cleared
	//
	class IntervalControl {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(IntervalControl);

		protected:
			// changed with criticalSection entered, read lock free
			TAtomic<bool> cleared;
			CriticalSection criticalSection;
			ConditionVariable conditionVariable;

		public:
			XYO_MULTITHREADING_EXPORT IntervalControl();

			XYO_MULTITHREADING_EXPORT void clear();
			XYO_MULTITHREADING_EXPORT bool isCleared() const;
			XYO_MULTITHREADING_EXPORT void reset();
			XYO_MULTITHREADING_EXPORT bool waitFor(int milliSeconds);
	};

};

#endif

#endif
