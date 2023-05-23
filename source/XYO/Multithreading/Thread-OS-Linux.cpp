// Multithreading
// Copyright (c) 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Dependency.hpp>
#include <XYO/Multithreading/Thread.hpp>

#ifdef XYO_OS_LINUX

#	include <pthread.h>
#	include <unistd.h>

#	ifdef XYO_SINGLE_THREAD
namespace XYO::Multithreading {

	void Thread::sleep(int milliSeconds) {		
		struct timespec _sleep;
		sleep.tv_sec = milliSeconds/1000;
		sleep.tv_nsec = (milliSeconds%1000)*1000000;
		while(nanosleep(&_sleep, &_sleep)){};
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
			bool isValid;
			pthread_t thread;
	};

	static void *threadProcedure_(Thread_ *thread) {
		RegistryThread::threadBegin();
		(*thread->procedure)(thread->this_);
		RegistryThread::threadEnd();
		thread->isValid = false;
		return nullptr;
	};

	Thread::Thread() {
		thread = new Thread_();
		thread->procedure = nullptr;
		thread->this_ = nullptr;
		thread->isValid = false;
	};

	Thread::~Thread() {
		join();
		delete thread;
	};

	void Thread::join() {
		if (thread->isValid) {
			if (pthread_join(thread->thread, nullptr)) {
				throw std::runtime_error("pthread_join");
			};
			thread->isValid = false;
		};
	};

	bool Thread::start(ThreadProcedure procedure, void *this_) {
		join();

		thread->procedure = procedure;
		thread->this_ = this_;

		if (pthread_create(&thread->thread, nullptr, (void *(*)(void *))threadProcedure_, thread) == 0) {
			thread->isValid = true;
			return true;
		};
		return false;
	};

	bool Thread::isRunning() {
		if (thread->isValid) {
			return true;
		};
		return false;
	};

	void Thread::sleep(int milliSeconds) {
		struct timespec _sleep;
		sleep.tv_sec = milliSeconds/1000;
		sleep.tv_nsec = (milliSeconds%1000)*1000000;
		while(nanosleep(&_sleep, &_sleep)){};
	};

};

#	endif

#endif
