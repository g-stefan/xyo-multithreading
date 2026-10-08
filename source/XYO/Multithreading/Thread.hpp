// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_THREAD_HPP
#define XYO_MULTITHREADING_THREAD_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifndef XYO_MULTITHREADING_INTERVALCONTROL_HPP
#	include <XYO/Multithreading/IntervalControl.hpp>
#endif

#ifdef XYO_PLATFORM_SINGLE_THREAD

namespace XYO::Multithreading::Thread {

	XYO_MULTITHREADING_EXPORT void sleep(int milliSeconds);

};

#endif

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	typedef Platform::Multithreading::ThreadProcedure ThreadProcedure;

	//
	// Thread - RAII owner of a system thread
	//
	// The thread that creates a Thread object is its owner:
	//   start(), join() and the destructor only from the owner thread.
	// Other threads may only read it: isRunning() is a single atomic read,
	// waitFinish() sleeps until the thread procedure ended.
	//
	// The destructor joins: destroying a Thread (or releasing the last
	// TPointer<Thread>) waits for the thread procedure to end.
	//
	// Helpers below start a new thread and return it, the returned
	// TPointer<Thread> is owned by the caller:
	// - keep it while the thread must run, releasing it waits for the end,
	//   releasing it right away (not stored) blocks the caller until done
	// - everything passed by pointer or reference (this_, clearInterval,
	//   thread) must outlive the returned thread; with RAII declare it
	//   before the TPointer<Thread>, objects are destroyed in reverse order
	// - returns nullptr if the thread can not be started
	//
	class Thread : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(Thread);

		protected:
			Platform::Multithreading::Thread thread;
			ThreadProcedure procedure;
			void *procedureThis;
			// isFinished is protected by finishSection,
			// finishCondition wakes waitFinish()
			CriticalSection finishSection;
			ConditionVariable finishCondition;
			bool isFinished;

			static void threadProcedure(void *this_);

		public:
			XYO_MULTITHREADING_EXPORT Thread();
			XYO_MULTITHREADING_EXPORT ~Thread();
			XYO_MULTITHREADING_EXPORT bool start(ThreadProcedure procedure, void *this_);
			XYO_MULTITHREADING_EXPORT void join();
			XYO_MULTITHREADING_EXPORT bool isRunning();
			// Sleep until the thread procedure ended (returned or thrown),
			// returns right away if not started; any thread, read only
			XYO_MULTITHREADING_EXPORT void waitFinish();
			XYO_MULTITHREADING_EXPORT static void sleep(int milliSeconds);

			// ---

			// Call procedure(this_) once, after milliSeconds
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> onTimeout(int milliSeconds, ThreadProcedure procedure, void *this_);

			// Call procedure(this_) every milliSeconds, first call after milliSeconds,
			// until clearInterval is set to true, no call after clear.
			// clearInterval is checked every 10 ms (it can not wake the thread),
			// stopping takes up to about 10 ms, use IntervalControl to stop right away
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> setInterval(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);

			// Call procedure(this_) every milliSeconds, first call after milliSeconds,
			// until control.clear(), no call after clear; clear() wakes the thread,
			// it stops right away, no polling
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> setInterval(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_);

			// Call procedure(this_) once, after the thread procedure of thread
			// ended, right away if thread is not started - start thread first.
			// Waits with thread.waitFinish(), no polling, several onFinish on
			// the same thread are all called.
			// thread is only read (waitFinish()), it stays owned by the caller
			// and must be destroyed after the returned thread:
			//
			//   Thread thread;                // declared first, destroyed last
			//   thread.start(...);
			//   TPointer<Thread> onFinish = Thread::onFinish(thread, ...);
			//
			// or release the returned thread first (onFinish = nullptr).
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> onFinish(Thread &thread, ThreadProcedure procedure, void *this_);

			// As setInterval, first call right away
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> setIntervalActionFirst(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> setIntervalActionFirst(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_);
	};

};

#endif

#endif
