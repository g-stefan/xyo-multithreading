// Multithreading
// Copyright (c) 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_THREAD_HPP
#define XYO_MULTITHREADING_THREAD_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifdef XYO_SINGLE_THREAD

namespace XYO::Multithreading {

	struct Thread {
			XYO_MULTITHREADING_EXPORT static void sleep(int milliSeconds);
	};

};

#endif

#ifdef XYO_MULTI_THREAD

namespace XYO::Multithreading {

	typedef void (*ThreadProcedure)(void *);

	class Thread_;

	class Thread : public Object {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Thread);

		protected:
			class Thread_ *thread;

		public:
			XYO_MULTITHREADING_EXPORT Thread();
			XYO_MULTITHREADING_EXPORT ~Thread();
			XYO_MULTITHREADING_EXPORT bool start(ThreadProcedure procedure, void *this_);
			XYO_MULTITHREADING_EXPORT void join();
			XYO_MULTITHREADING_EXPORT bool isRunning();
			XYO_MULTITHREADING_EXPORT static void sleep(int milliSeconds);

			// ---

			XYO_MULTITHREADING_EXPORT static TPointer<Thread> onTimeout(int milliSeconds, ThreadProcedure procedure, void *this_);
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> setInterval(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> onFinish(Thread &thread, ThreadProcedure procedure, void *this_);
			XYO_MULTITHREADING_EXPORT static TPointer<Thread> setIntervalActionFirst(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
	};

};

#endif

#endif
