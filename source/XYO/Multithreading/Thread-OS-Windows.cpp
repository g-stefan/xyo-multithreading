// Multithreading
// Copyright (c) 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Dependency.hpp>
#include <XYO/Multithreading/Thread.hpp>

#ifdef XYO_OS_WINDOWS

#	include <stdio.h>
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>

#	ifdef XYO_SINGLE_THREAD
namespace XYO::Multithreading {

	void Thread::sleep(int milliSeconds) {
		Sleep(milliSeconds);
	};

};
#	endif

#	ifdef XYO_MULTI_THREAD
namespace XYO::Multithreading {

	class Thread_ {
		public:
			ThreadProcedure procedure;
			void *this_;
			//
			DWORD id;
			HANDLE thread;
	};

	static DWORD WINAPI threadProcedure_(Thread_ *thread) {
		RegistryThread::threadBegin();
		(*thread->procedure)(thread->this_);
		RegistryThread::threadEnd();
		thread->id = 0;
		return 0L;
	};

	Thread::Thread() {
		thread = new Thread_();
		thread->procedure = nullptr;
		thread->this_ = nullptr;
		thread->id = 0;
		thread->thread = nullptr;
	};

	Thread::~Thread() {
		join();
		delete thread;
	};

	void Thread::join() {
		if (thread->id) {
			WaitForSingleObject(thread->thread, INFINITE);
			CloseHandle(thread->thread);
			thread->id = 0;
			thread->thread = nullptr;
		};
	};

	bool Thread::start(ThreadProcedure procedure, void *this_) {
		join();
		thread->procedure = procedure;
		thread->this_ = this_;

		thread->thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)threadProcedure_, thread, 0, &thread->id);
		if (thread->thread != nullptr) {
			return true;
		};
		return false;
	};

	bool Thread::isRunning() {
		if (thread->id) {
			return true;
		};
		return false;
	};

	void Thread::sleep(int milliSeconds) {
		Sleep(milliSeconds);
	};

};
#	endif

#endif
