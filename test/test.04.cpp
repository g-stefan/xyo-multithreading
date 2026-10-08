// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>
#include <chrono>

using namespace XYO::Multithreading;

// Thread::setInterval / Thread::setIntervalActionFirst
//
// - no call after the interval is cleared
// - clear stops the thread without waiting the whole interval
// - interval between calls is respected
//
// With IntervalControl
//
// - no call after clear(), also clear() from the callback
// - clear() wakes the thread, stops right away (no 10 ms polling)
// - one control stops several intervals
// - reset() to use the control again

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

typedef std::chrono::steady_clock Clock;

int elapsedMilliSeconds(Clock::time_point begin) {
	return (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - begin).count();
};

TAtomic<int> callCount;

void countProcedure(void *) {
	callCount.fetchAdd(1);
};

void waitCallCount(int count) {
	while (callCount.get() < count) {
		Thread::sleep(1);
	};
};

void testNoCallAfterClear() {
	TAtomic<bool> clearInterval(false);
	TPointer<Thread> thread;
	char message[256];
	int countAtClear;

	callCount.set(0);
	thread = Thread::setInterval(clearInterval, 100, countProcedure, nullptr);
	check(thread, "setInterval: start");
	if (!thread) {
		return;
	};

	// clear while thread waits for next call
	waitCallCount(3);
	clearInterval.set(true);
	countAtClear = callCount.get();
	thread->join();

	sprintf(message, "setInterval: calls at clear %d, after join %d", countAtClear, callCount.get());
	check(callCount.get() == countAtClear, message);
};

void testStopWithoutWaitingInterval() {
	TAtomic<bool> clearInterval(false);
	TPointer<Thread> thread;
	Clock::time_point begin;
	char message[256];
	int stopTime;

	callCount.set(0);
	thread = Thread::setInterval(clearInterval, 5000, countProcedure, nullptr);
	check(thread, "setInterval, long interval: start");
	if (!thread) {
		return;
	};

	Thread::sleep(50);
	begin = Clock::now();
	clearInterval.set(true);
	thread->join();
	stopTime = elapsedMilliSeconds(begin);

	sprintf(message, "setInterval, long interval: stop in %d ms, calls %d", stopTime, callCount.get());
	check((stopTime < 1000) && (callCount.get() == 0), message);
};

void testActionFirst() {
	TAtomic<bool> clearInterval(false);
	TPointer<Thread> thread;
	Clock::time_point begin;
	char message[256];
	int stopTime;

	callCount.set(0);
	thread = Thread::setIntervalActionFirst(clearInterval, 5000, countProcedure, nullptr);
	check(thread, "setIntervalActionFirst, long interval: start");
	if (!thread) {
		return;
	};

	// first call is done at start
	waitCallCount(1);
	begin = Clock::now();
	clearInterval.set(true);
	thread->join();
	stopTime = elapsedMilliSeconds(begin);

	sprintf(message, "setIntervalActionFirst, long interval: stop in %d ms, calls %d", stopTime, callCount.get());
	check((stopTime < 1000) && (callCount.get() == 1), message);
};

void testInterval() {
	TAtomic<bool> clearInterval(false);
	TPointer<Thread> thread;
	Clock::time_point begin;
	char message[256];
	int time;

	callCount.set(0);
	begin = Clock::now();
	thread = Thread::setInterval(clearInterval, 100, countProcedure, nullptr);
	check(thread, "setInterval, interval: start");
	if (!thread) {
		return;
	};

	waitCallCount(3);
	time = elapsedMilliSeconds(begin);
	clearInterval.set(true);
	thread->join();

	sprintf(message, "setInterval, interval: 3 calls of 100 ms in %d ms", time);
	check(time >= 300, message);
};

// --- IntervalControl

struct ControlCount {
		IntervalControl control;
		TAtomic<int> count;
		int clearAt;

		ControlCount() {
			count.set(0);
			clearAt = 0;
		};

		void waitCount(int value) {
			while (count.get() < value) {
				Thread::sleep(1);
			};
		};
};

void controlCountProcedure(void *this__) {
	ControlCount *this_ = reinterpret_cast<ControlCount *>(this__);
	int value = this_->count.fetchAdd(1) + 1;
	if (value == this_->clearAt) {
		this_->control.clear();
	};
};

void testControlNoCallAfterClear() {
	ControlCount controlCount;
	char message[256];
	int countAtClear;

	TPointer<Thread> thread = Thread::setInterval(controlCount.control, 100, controlCountProcedure, &controlCount);
	check(thread, "control, setInterval: start");
	if (!thread) {
		return;
	};
	controlCount.waitCount(3);
	controlCount.control.clear();
	countAtClear = controlCount.count.get();
	thread->join();

	sprintf(message, "control, setInterval: calls at clear %d, after join %d", countAtClear, controlCount.count.get());
	check(controlCount.count.get() == countAtClear, message);
};

void testControlClearInCallback() {
	ControlCount controlCount;
	char message[256];

	controlCount.clearAt = 3;
	TPointer<Thread> thread = Thread::setInterval(controlCount.control, 10, controlCountProcedure, &controlCount);
	check(thread, "control, clear() from callback: start");
	if (!thread) {
		return;
	};
	thread->join();

	sprintf(message, "control, clear() from the 3rd callback: calls %d", controlCount.count.get());
	check(controlCount.count.get() == 3, message);
};

void testControlStopTime() {
	const int rounds = 20;
	char message[256];
	long long total = 0;
	bool isOk = true;
	int k;

	for (k = 0; k < rounds; ++k) {
		ControlCount controlCount;
		TPointer<Thread> thread;
		if (k & 1) {
			thread = Thread::setInterval(controlCount.control, 5000, controlCountProcedure, &controlCount);
		} else {
			thread = Thread::setIntervalActionFirst(controlCount.control, 5000, controlCountProcedure, &controlCount);
		};
		if (!thread) {
			isOk = false;
			continue;
		};
		// thread is waiting the interval
		Thread::sleep(20);
		Clock::time_point begin = Clock::now();
		controlCount.control.clear();
		thread->join();
		total += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count();
		if (controlCount.count.get() != ((k & 1) ? 0 : 1)) {
			isOk = false;
		};
	};

	// polling every 10 ms: on average more than 5000 us
	sprintf(message, "control, interval 5000 ms: stop in %lld us on average, %d rounds, no polling", total / rounds, rounds);
	check(isOk && ((total / rounds) < 2000), message);
};

void testControlSeveral() {
	const int threadCount = 3;
	ControlCount controlCount;
	TPointer<Thread> threads[threadCount];
	Clock::time_point begin;
	char message[256];
	int time;
	int k;

	threads[0] = Thread::setInterval(controlCount.control, 5000, controlCountProcedure, &controlCount);
	threads[1] = Thread::setInterval(controlCount.control, 5000, controlCountProcedure, &controlCount);
	threads[2] = Thread::setIntervalActionFirst(controlCount.control, 5000, controlCountProcedure, &controlCount);
	controlCount.waitCount(1);
	Thread::sleep(20);

	begin = Clock::now();
	controlCount.control.clear();
	for (k = 0; k < threadCount; ++k) {
		threads[k] = nullptr;
	};
	time = elapsedMilliSeconds(begin);

	sprintf(message, "control, one clear() stops %d intervals in %d ms, calls %d", threadCount, time, controlCount.count.get());
	check((time < 1000) && (controlCount.count.get() == 1), message);
};

void testControlReset() {
	ControlCount controlCount;
	TPointer<Thread> thread;

	controlCount.control.clear();
	thread = Thread::setInterval(controlCount.control, 10, controlCountProcedure, &controlCount);
	thread = nullptr;
	check(controlCount.count.get() == 0, "control cleared before start: no call");

	controlCount.control.reset();
	check(!controlCount.control.isCleared(), "control reset()");
	thread = Thread::setInterval(controlCount.control, 10, controlCountProcedure, &controlCount);
	controlCount.waitCount(2);
	controlCount.control.clear();
	thread = nullptr;
	check(controlCount.count.get() >= 2, "control used again after reset()");
};

void testControlInterval() {
	ControlCount controlCount;
	Clock::time_point begin;
	char message[256];
	int time;

	begin = Clock::now();
	TPointer<Thread> thread = Thread::setInterval(controlCount.control, 100, controlCountProcedure, &controlCount);
	controlCount.waitCount(3);
	time = elapsedMilliSeconds(begin);
	controlCount.control.clear();
	thread = nullptr;

	sprintf(message, "control, interval: 3 calls of 100 ms in %d ms", time);
	check(time >= 300, message);
};

void test() {
	printf("- Thread::setInterval\r\n");
	testNoCallAfterClear();
	testStopWithoutWaitingInterval();
	testActionFirst();
	testInterval();

	printf("- Thread::setInterval with IntervalControl\r\n");
	testControlNoCallAfterClear();
	testControlClearInCallback();
	testControlStopTime();
	testControlSeveral();
	testControlReset();
	testControlInterval();

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
