// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>
#include <cstdlib>
#include <chrono>
#include <stdexcept>

using namespace XYO::Multithreading;

// Thread::onFinish - RAII use
//
// The owner thread keeps both the monitored Thread and the returned
// TPointer<Thread>, the monitor thread only reads the monitored Thread
// (waitFinish()). The monitored Thread must be destroyed after the monitor:
// declared before it, or the monitor joined / released first.
//
// - scope, monitored Thread declared first
// - class members, monitored Thread declared first
// - monitor released first, any declaration order
// - several monitors on the same Thread
// - monitored Thread not running, procedure called right away
// - thread procedure throws, monitor is called
// - Thread started again, monitor of each run
// - monitor wakes up without polling

typedef std::chrono::steady_clock Clock;

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

// A monitor that never ends makes the test hang,
// the watchdog fails the test instead

struct Watchdog {
		TAtomic<bool> done;
};

void watchdogProcedure(void *this__) {
	const int timeLimit = 60000;
	Watchdog *this_ = reinterpret_cast<Watchdog *>(this__);
	int k;

	for (k = 0; k < timeLimit; k += 10) {
		if (this_->done.get()) {
			return;
		};
		Thread::sleep(10);
	};

	printf("[FAIL] test did not end in %d ms\r\n", timeLimit);
	printf("* Error: test failed\n");
	fflush(stdout);
	std::_Exit(1);
};

// ---

// Must outlive the monitored Thread and the monitor, declared first
struct Work {
		int milliSeconds;
		TAtomic<bool> workDone;
		TAtomic<int> finishCount;
		TAtomic<bool> finishAfterWork;

		Work() {
			milliSeconds = 0;
			workDone.set(false);
			finishCount.set(0);
			finishAfterWork.set(true);
		};
};

void workProcedure(void *this__) {
	Work *this_ = reinterpret_cast<Work *>(this__);
	Thread::sleep(this_->milliSeconds);
	this_->workDone.set(true);
};

void finishProcedure(void *this__) {
	Work *this_ = reinterpret_cast<Work *>(this__);
	if (!this_->workDone.get()) {
		this_->finishAfterWork.set(false);
	};
	this_->finishCount.fetchAdd(1);
};

bool isFinishOk(Work &work, int count) {
	return work.workDone.get() && work.finishAfterWork.get() && (work.finishCount.get() == count);
};

const int rounds = 200;

void testScope() {
	bool isOk = true;
	int k;

	for (k = 0; k < rounds; ++k) {
		Work work;
		work.milliSeconds = k % 3;
		{
			Thread thread;
			if (!thread.start(workProcedure, &work)) {
				isOk = false;
				continue;
			};
			TPointer<Thread> onFinish = Thread::onFinish(thread, finishProcedure, &work);
			if (!onFinish) {
				isOk = false;
			};
			// destroyed in reverse order: onFinish joined first, then thread
		};
		if (!isFinishOk(work, 1)) {
			isOk = false;
		};
	};
	check(isOk, "scope, monitored thread declared first");
};

struct Task {
		// declaration order is destruction order reversed
		Thread thread;
		TPointer<Thread> onFinish;

		bool start(Work &work) {
			if (!thread.start(workProcedure, &work)) {
				return false;
			};
			onFinish = Thread::onFinish(thread, finishProcedure, &work);
			return onFinish;
		};
};

void testClassMembers() {
	bool isOk = true;
	int k;

	for (k = 0; k < rounds; ++k) {
		Work work;
		work.milliSeconds = k % 3;
		{
			Task task;
			if (!task.start(work)) {
				isOk = false;
			};
		};
		if (!isFinishOk(work, 1)) {
			isOk = false;
		};
	};
	check(isOk, "class members, monitored thread declared first");
};

void testReleaseMonitorFirst() {
	bool isOk = true;
	int k;

	for (k = 0; k < rounds; ++k) {
		Work work;
		work.milliSeconds = k % 3;
		{
			TPointer<Thread> onFinish;
			Thread thread;
			if (!thread.start(workProcedure, &work)) {
				isOk = false;
				continue;
			};
			onFinish = Thread::onFinish(thread, finishProcedure, &work);
			// declared in wrong order, release monitor first
			onFinish = nullptr;
		};
		if (!isFinishOk(work, 1)) {
			isOk = false;
		};
	};
	check(isOk, "monitor released first, monitored thread declared last");
};

void testSeveralMonitors() {
	const int monitors = 4;
	bool isOk = true;
	int k;
	int m;

	for (k = 0; k < rounds / 4; ++k) {
		Work work;
		work.milliSeconds = 1 + (k % 3);
		{
			Thread thread;
			TPointer<Thread> onFinish[monitors];
			if (!thread.start(workProcedure, &work)) {
				isOk = false;
				continue;
			};
			for (m = 0; m < monitors; ++m) {
				onFinish[m] = Thread::onFinish(thread, finishProcedure, &work);
			};
		};
		if (!isFinishOk(work, monitors)) {
			isOk = false;
		};
	};
	check(isOk, "several monitors on the same thread, each called once");
};

void testNotRunning() {
	Work work;
	Thread thread;
	TPointer<Thread> onFinish;

	onFinish = Thread::onFinish(thread, finishProcedure, &work);
	check(onFinish, "monitored thread not running: start monitor");
	if (!onFinish) {
		return;
	};
	onFinish->join();
	check(work.finishCount.get() == 1, "monitored thread not running: procedure called right away");
};

void throwProcedure(void *this__) {
	Work *this_ = reinterpret_cast<Work *>(this__);
	Thread::sleep(this_->milliSeconds);
	this_->workDone.set(true);
	throw std::runtime_error("thread procedure");
};

void testThrow() {
	Work work;
	work.milliSeconds = 5;
	{
		Thread thread;
		if (!thread.start(throwProcedure, &work)) {
			check(false, "thread procedure throws: start thread");
			return;
		};
		TPointer<Thread> onFinish = Thread::onFinish(thread, finishProcedure, &work);
	};
	check(isFinishOk(work, 1), "thread procedure throws, monitor is called");
};

void testStartAgain() {
	Work work1;
	Work work2;
	Thread thread;
	TPointer<Thread> onFinish;

	work1.milliSeconds = 5;
	work2.milliSeconds = 5;
	thread.start(workProcedure, &work1);
	onFinish = Thread::onFinish(thread, finishProcedure, &work1);
	onFinish = nullptr;
	thread.start(workProcedure, &work2);
	onFinish = Thread::onFinish(thread, finishProcedure, &work2);
	onFinish = nullptr;
	check(isFinishOk(work1, 1) && isFinishOk(work2, 1), "thread started again, monitor of each run called once");
};

// ---

struct TimedWork {
		Clock::time_point workEnd;
		Clock::time_point finishCalled;
};

void timedWorkProcedure(void *this__) {
	TimedWork *this_ = reinterpret_cast<TimedWork *>(this__);
	// monitor is waiting when the work ends
	Thread::sleep(5);
	this_->workEnd = Clock::now();
};

void timedFinishProcedure(void *this__) {
	TimedWork *this_ = reinterpret_cast<TimedWork *>(this__);
	this_->finishCalled = Clock::now();
};

void testWakeUpTime() {
	const int count = 100;
	char message[256];
	long long total = 0;
	int k;

	for (k = 0; k < count; ++k) {
		TimedWork timedWork;
		{
			Thread thread;
			thread.start(timedWorkProcedure, &timedWork);
			TPointer<Thread> onFinish = Thread::onFinish(thread, timedFinishProcedure, &timedWork);
		};
		total += std::chrono::duration_cast<std::chrono::microseconds>(timedWork.finishCalled - timedWork.workEnd).count();
	};

	// polling with sleep(1): on average more than 500 us
	sprintf(message, "monitor called on average %lld us after the work end, %d rounds, no polling", total / count, count);
	check((total / count) < 300, message);
};

void test() {
	Watchdog watchdog;
	Thread watchdogThread;

	// keep output if process crashes
	setvbuf(stdout, nullptr, _IONBF, 0);

	watchdog.done.set(false);
	watchdogThread.start(watchdogProcedure, &watchdog);

	printf("- Thread::onFinish\r\n");
	testScope();
	testClassMembers();
	testReleaseMonitorFirst();
	testSeveralMonitors();
	testNotRunning();
	testThrow();
	testStartAgain();
	testWakeUpTime();

	watchdog.done.set(true);
	watchdogThread.join();

	if (errorCount) {
		throw std::runtime_error("test failed");
	};

	printf("Done.\r\n");
};

int main(int cmdN, char *cmdS[]) {
	try {

		test();

		return 0;

	} catch (const std::exception &e) {
		printf("* Error: %s\n", e.what());
	} catch (...) {
		printf("* Error: Unknown\n");
	};

	return 1;
};
