// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Dependency.hpp>
#include <XYO/Multithreading/Thread.hpp>
#include <XYO/Multithreading/CriticalSectionLock.hpp>

#include <chrono>

#ifdef XYO_PLATFORM_SINGLE_THREAD
namespace XYO::Multithreading::Thread {

	void sleep(int milliSeconds) {
		XYO::Platform::Multithreading::Thread::sleep(milliSeconds);
	};

};
#endif

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	Thread::Thread() {
		procedure = nullptr;
		procedureThis = nullptr;
		isFinished = true;
	};

	// Join here, the thread procedure uses finishSection / finishCondition,
	// members destroyed before the thread member (it would join too late)
	Thread::~Thread() {
		thread.join();
	};

	void Thread::join() {
		thread.join();
	};

	// Marks the end of the thread procedure, also when it throws
	class ThreadFinish_ {
		public:
			CriticalSection &finishSection;
			ConditionVariable &finishCondition;
			bool &isFinished;

			ThreadFinish_(CriticalSection &finishSection_, ConditionVariable &finishCondition_, bool &isFinished_)
			    : finishSection(finishSection_), finishCondition(finishCondition_), isFinished(isFinished_){};

			~ThreadFinish_() {
				CriticalSectionLock lock(finishSection);
				isFinished = true;
				finishCondition.notifyAll();
			};
	};

	void Thread::threadProcedure(void *this__) {
		Thread *this_ = reinterpret_cast<Thread *>(this__);
		ThreadFinish_ finish(this_->finishSection, this_->finishCondition, this_->isFinished);
		(*this_->procedure)(this_->procedureThis);
	};

	bool Thread::start(ThreadProcedure procedure_, void *this_) {
		// previous run must end first, it would mark the new run finished
		thread.join();

		procedure = procedure_;
		procedureThis = this_;
		{
			CriticalSectionLock lock(finishSection);
			isFinished = false;
		};

		if (thread.start(threadProcedure, this, RegistryThread::threadBegin, RegistryThread::threadEnd)) {
			return true;
		};

		CriticalSectionLock lock(finishSection);
		isFinished = true;
		finishCondition.notifyAll();
		return false;
	};

	bool Thread::isRunning() {
		return thread.isRunning();
	};

	void Thread::waitFinish() {
		CriticalSectionLock lock(finishSection);
		while (!isFinished) {
			finishCondition.wait(finishSection);
		};
	};

	void Thread::sleep(int milliSeconds) {
		Platform::Multithreading::Thread::sleep(milliSeconds);
	};

	// ---
	// Object reference count is not atomic, the thread procedure is the only
	// owner of the thread data, the caller must not access it after start().

	class ThreadTimeout : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			int milliSeconds;
	};

	static void onTimeoutProcedure(void *this__) {
		TPointer<ThreadTimeout> this_(reinterpret_cast<ThreadTimeout *>(this__));
		Thread::sleep(this_->milliSeconds);
		(*this_->procedure)(this_->this_);
	};

	TPointer<Thread> Thread::onTimeout(int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<Thread> thread;
		ThreadTimeout *threadTimeout;

		thread.newMemory();

		threadTimeout = TMemorySystem<ThreadTimeout>::newMemory();
		threadTimeout->procedure = procedure;
		threadTimeout->this_ = this_;
		threadTimeout->milliSeconds = milliSeconds;

		if (thread->start(onTimeoutProcedure, threadTimeout)) {
			return thread;
		};
		TMemorySystem<ThreadTimeout>::deleteMemory(threadTimeout);

		return nullptr;
	};

	// ---

	class ThreadInterval : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			int milliSeconds;
			TAtomic<bool> *clearInterval;
	};

	// Wait milliSeconds or until interval is cleared,
	// return false if interval is cleared
	static bool waitInterval(TAtomic<bool> *clearInterval, int milliSeconds) {
		// short sleeps, stop soon after clear
		const int slice = 10;

		if (milliSeconds <= 0) {
			Thread::sleep(milliSeconds);
			return !clearInterval->get();
		};

		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliSeconds);
		for (;;) {
			if (clearInterval->get()) {
				return false;
			};
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			if (now >= end) {
				return true;
			};
			int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
			Thread::sleep(remaining < slice ? remaining : slice);
		};
	};

	static void onIntervalProcedure(void *this__) {
		TPointer<ThreadInterval> this_(reinterpret_cast<ThreadInterval *>(this__));
		while (waitInterval(this_->clearInterval, this_->milliSeconds)) {
			(*this_->procedure)(this_->this_);
		};
	};

	TPointer<Thread> Thread::setInterval(Platform::Multithreading::TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<Thread> thread;
		ThreadInterval *threadInterval;

		thread.newMemory();

		threadInterval = TMemorySystem<ThreadInterval>::newMemory();
		threadInterval->procedure = procedure;
		threadInterval->this_ = this_;
		threadInterval->milliSeconds = milliSeconds;
		threadInterval->clearInterval = &clearInterval;

		if (thread->start(onIntervalProcedure, threadInterval)) {
			return thread;
		};
		TMemorySystem<ThreadInterval>::deleteMemory(threadInterval);

		return nullptr;
	};

	// ---

	class ThreadFinish : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			Thread *thread;
	};

	static void onFinishProcedure(void *this__) {
		TPointer<ThreadFinish> this_(reinterpret_cast<ThreadFinish *>(this__));
		this_->thread->waitFinish();
		(*this_->procedure)(this_->this_);
	};

	TPointer<Thread> Thread::onFinish(Thread &thread_, ThreadProcedure procedure, void *this_) {
		TPointer<Thread> thread;
		ThreadFinish *threadFinish;

		thread.newMemory();

		threadFinish = TMemorySystem<ThreadFinish>::newMemory();
		threadFinish->procedure = procedure;
		threadFinish->this_ = this_;
		threadFinish->thread = &thread_;

		if (thread->start(onFinishProcedure, threadFinish)) {
			return thread;
		};
		TMemorySystem<ThreadFinish>::deleteMemory(threadFinish);

		return nullptr;
	};

	// ---

	static void onIntervalActionFirstProcedure(void *this__) {
		TPointer<ThreadInterval> this_(reinterpret_cast<ThreadInterval *>(this__));
		while (this_->clearInterval->get() == false) {
			(*this_->procedure)(this_->this_);
			waitInterval(this_->clearInterval, this_->milliSeconds);
		};
	};

	TPointer<Thread> Thread::setIntervalActionFirst(Platform::Multithreading::TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<Thread> thread;
		ThreadInterval *threadInterval;

		thread.newMemory();

		threadInterval = TMemorySystem<ThreadInterval>::newMemory();
		threadInterval->procedure = procedure;
		threadInterval->this_ = this_;
		threadInterval->milliSeconds = milliSeconds;
		threadInterval->clearInterval = &clearInterval;

		if (thread->start(onIntervalActionFirstProcedure, threadInterval)) {
			return thread;
		};
		TMemorySystem<ThreadInterval>::deleteMemory(threadInterval);

		return nullptr;
	};

	// --- IntervalControl, clear() wakes the waiting thread

	class ThreadIntervalControl : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			int milliSeconds;
			IntervalControl *control;
	};

	static void onIntervalControlProcedure(void *this__) {
		TPointer<ThreadIntervalControl> this_(reinterpret_cast<ThreadIntervalControl *>(this__));
		while (this_->control->waitFor(this_->milliSeconds)) {
			(*this_->procedure)(this_->this_);
		};
	};

	static void onIntervalControlActionFirstProcedure(void *this__) {
		TPointer<ThreadIntervalControl> this_(reinterpret_cast<ThreadIntervalControl *>(this__));
		while (!this_->control->isCleared()) {
			(*this_->procedure)(this_->this_);
			if (!this_->control->waitFor(this_->milliSeconds)) {
				break;
			};
		};
	};

	static TPointer<Thread> startIntervalControl(ThreadProcedure threadProcedure, IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<Thread> thread;
		ThreadIntervalControl *threadInterval;

		thread.newMemory();

		threadInterval = TMemorySystem<ThreadIntervalControl>::newMemory();
		threadInterval->procedure = procedure;
		threadInterval->this_ = this_;
		threadInterval->milliSeconds = milliSeconds;
		threadInterval->control = &control;

		if (thread->start(threadProcedure, threadInterval)) {
			return thread;
		};
		TMemorySystem<ThreadIntervalControl>::deleteMemory(threadInterval);

		return nullptr;
	};

	TPointer<Thread> Thread::setInterval(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_) {
		return startIntervalControl(onIntervalControlProcedure, control, milliSeconds, procedure, this_);
	};

	TPointer<Thread> Thread::setIntervalActionFirst(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_) {
		return startIntervalControl(onIntervalControlActionFirstProcedure, control, milliSeconds, procedure, this_);
	};
};

#endif
