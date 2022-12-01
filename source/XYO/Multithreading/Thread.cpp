// Multithreading
// Copyright (c) 2022 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2022 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Dependency.hpp>
#include <XYO/Multithreading/Thread.hpp>

#ifdef XYO_MULTI_THREAD

namespace XYO::Multithreading {

	class ThreadTimeout : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			int milliSeconds;
	};

	static void onTimeoutProcedure(void *this__) {
		ThreadTimeout *this_ = reinterpret_cast<ThreadTimeout *>(this__);
		Thread::sleep(this_->milliSeconds);
		(*this_->procedure)(this_->this_);
		this_->decReferenceCount();
	};

	TPointer<Thread> Thread::onTimeout(int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<ThreadTimeout> threadTimeout(TMemorySystem<ThreadTimeout>::newMemory());
		TPointer<Thread> thread;

		thread.newMemory();

		threadTimeout->procedure = procedure;
		threadTimeout->this_ = this_;
		threadTimeout->milliSeconds = milliSeconds;

		threadTimeout->incReferenceCount();
		if (thread->start(onTimeoutProcedure, threadTimeout)) {
			return thread;
		};
		threadTimeout->decReferenceCount();

		return nullptr;
	};

	class ThreadInterval : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			int milliSeconds;
			TAtomic<bool> *clearInterval;
	};

	static void onIntervalProcedure(void *this__) {
		ThreadInterval *this_ = reinterpret_cast<ThreadInterval *>(this__);
		while (this_->clearInterval->get() == false) {
			Thread::sleep(this_->milliSeconds);
			(*this_->procedure)(this_->this_);
		};
		this_->decReferenceCount();
	};

	TPointer<Thread> Thread::setInterval(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<ThreadInterval> threadInterval(TMemorySystem<ThreadInterval>::newMemory());
		TPointer<Thread> thread;

		thread.newMemory();

		threadInterval->procedure = procedure;
		threadInterval->this_ = this_;
		threadInterval->milliSeconds = milliSeconds;
		threadInterval->clearInterval = &clearInterval;

		threadInterval->incReferenceCount();
		if (thread->start(onIntervalProcedure, threadInterval)) {
			return thread;
		};
		threadInterval->decReferenceCount();

		return nullptr;
	};

	class ThreadFinish : public Object {
		public:
			ThreadProcedure procedure;
			void *this_;
			Thread *thread;
	};

	static void onFinishProcedure(void *this__) {
		ThreadFinish *this_ = reinterpret_cast<ThreadFinish *>(this__);
		while (this_->thread->isRunning()) {
			Thread::sleep(1);
		};
		(*this_->procedure)(this_->this_);
		this_->decReferenceCount();
	};

	TPointer<Thread> Thread::onFinish(Thread &thread_, ThreadProcedure procedure, void *this_) {
		TPointer<ThreadFinish> threadFinish(TMemorySystem<ThreadFinish>::newMemory());
		TPointer<Thread> thread;

		thread.newMemory();

		threadFinish->procedure = procedure;
		threadFinish->this_ = this_;
		threadFinish->thread = &thread_;

		threadFinish->incReferenceCount();
		if (thread->start(onFinishProcedure, threadFinish)) {
			return thread;
		};
		threadFinish->decReferenceCount();

		return nullptr;
	};

	static void onIntervalActionFirstProcedure(void *this__) {
		ThreadInterval *this_ = reinterpret_cast<ThreadInterval *>(this__);
		while (this_->clearInterval->get() == false) {
			(*this_->procedure)(this_->this_);
			Thread::sleep(this_->milliSeconds);
		};
		this_->decReferenceCount();
	};

	TPointer<Thread> Thread::setIntervalActionFirst(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_) {
		TPointer<ThreadInterval> threadInterval(TMemorySystem<ThreadInterval>::newMemory());
		TPointer<Thread> thread;

		thread.newMemory();

		threadInterval->procedure = procedure;
		threadInterval->this_ = this_;
		threadInterval->milliSeconds = milliSeconds;
		threadInterval->clearInterval = &clearInterval;

		threadInterval->incReferenceCount();
		if (thread->start(onIntervalActionFirstProcedure, threadInterval)) {
			return thread;
		};
		threadInterval->decReferenceCount();

		return nullptr;
	};
};

#endif
