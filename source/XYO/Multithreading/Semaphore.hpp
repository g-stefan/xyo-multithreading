// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_SEMAPHORE_HPP
#define XYO_MULTITHREADING_SEMAPHORE_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	//
	// Binary signal between exactly two threads:
	//   one thread calls notify() - the writer,
	//   one thread calls wait() / peek() - the reader.
	// No other locking is required for this use.
	// Only one thread may wait() / waitFor(), notify() and peek() are
	// thread safe, calling them from other threads too is harmless.
	//
	// - notify() sets the signal, several notify() before wait() count as one
	// - wait() blocks until the signal is set, then clears it (consumes it),
	//   a notify() done before wait() is not lost
	// - waitFor() as wait(), at most milliSeconds, returns false on timeout
	// - peek() returns the signal state without clearing it, lock free,
	//   can be used as a stop flag: notify() once, peek() many times
	// - reset() clears the signal
	//
	// Waiting thread sleeps (no CPU, no polling) on a ConditionVariable
	// and is woken up by notify()
	//
	class Semaphore {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(Semaphore);

		protected:
			TAtomic<bool> state;
			CriticalSection criticalSection;
			ConditionVariable conditionVariable;

		public:
			XYO_MULTITHREADING_EXPORT Semaphore();

			XYO_MULTITHREADING_EXPORT void wait();
			XYO_MULTITHREADING_EXPORT bool waitFor(int milliSeconds);
			XYO_MULTITHREADING_EXPORT void notify();
			XYO_MULTITHREADING_EXPORT bool peek() const;
			XYO_MULTITHREADING_EXPORT void reset();
	};

};

#endif

#endif
