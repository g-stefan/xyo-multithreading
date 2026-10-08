// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>
#include <cstdlib>

using namespace XYO::Multithreading;

// CriticalSectionLock and Synchronize
//
// - return types: value, void, reference, type without default constructor
// - any callable: lambda with large capture, std::function
// - lock released when fn() throws
// - mutual exclusion between threads

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

// A lock not released makes the test hang,
// the watchdog fails the test instead

struct Watchdog {
		TAtomic<bool> done;
};

void watchdogProcedure(void *this__) {
	const int timeLimit = 30000;
	Watchdog *this_ = reinterpret_cast<Watchdog *>(this__);
	int k;

	for (k = 0; k < timeLimit; k += 10) {
		if (this_->done.get()) {
			return;
		};
		Thread::sleep(10);
	};

	printf("[FAIL] test did not end in %d ms, a lock was not released\r\n", timeLimit);
	printf("* Error: test failed\n");
	fflush(stdout);
	std::_Exit(1);
};

// ---

struct NoDefaultConstructor {
		int value;

		NoDefaultConstructor(int value_) : value(value_) {};
};

void testReturnTypes() {
	CriticalSection criticalSection;
	int counter = 0;

	int value = Synchronize<int>::process(criticalSection, [&]() {
		return ++counter;
	});
	check((value == 1) && (counter == 1), "return value");

	Synchronize<void>::process(criticalSection, [&]() {
		++counter;
	});
	check(counter == 2, "return void");

	int &reference = Synchronize<int &>::process(criticalSection, [&]() -> int & {
		return counter;
	});
	reference = 10;
	check(counter == 10, "return reference");

	NoDefaultConstructor noDefault = Synchronize<NoDefaultConstructor>::process(criticalSection, [&]() {
		return NoDefaultConstructor(counter + 1);
	});
	check(noDefault.value == 11, "return type without default constructor");
};

void testCallables() {
	CriticalSection criticalSection;
	char buffer[256];
	int k;

	for (k = 0; k < 256; ++k) {
		buffer[k] = (char)k;
	};
	int sum = Synchronize<int>::process(criticalSection, [buffer]() {
		int retV = 0;
		for (int m = 0; m < 256; ++m) {
			retV += (unsigned char)buffer[m];
		};
		return retV;
	});
	check(sum == 255 * 256 / 2, "lambda with large capture");

	std::function<int()> fn = []() {
		return 42;
	};
	check(Synchronize<int>::process(criticalSection, fn) == 42, "std::function");
};

// ---

struct LockAfterThrow {
		CriticalSection *criticalSection;
		TAtomic<bool> entered;
};

void lockAfterThrowProcedure(void *this__) {
	LockAfterThrow *this_ = reinterpret_cast<LockAfterThrow *>(this__);
	CriticalSectionLock lock(*this_->criticalSection);
	this_->entered.set(true);
};

void testException() {
	CriticalSection criticalSection;
	LockAfterThrow lockAfterThrow;
	Thread thread;
	bool isThrown = false;

	try {
		Synchronize<int>::process(criticalSection, []() -> int {
			throw std::runtime_error("inside critical section");
		});
	} catch (const std::exception &) {
		isThrown = true;
	};
	check(isThrown, "exception passed to caller");

	// critical sections can be entered again by the same thread,
	// check the lock was released from another thread
	lockAfterThrow.criticalSection = &criticalSection;
	lockAfterThrow.entered.set(false);
	if (thread.start(lockAfterThrowProcedure, &lockAfterThrow)) {
		thread.join();
	};
	check(lockAfterThrow.entered.get(), "lock released after exception, other thread can enter");
};

// ---

const int increments = 10000;

struct Counter {
		CriticalSection criticalSection;
		int value;
};

void incrementProcedure(void *this__) {
	Counter *this_ = reinterpret_cast<Counter *>(this__);
	int k;
	for (k = 0; k < increments; ++k) {
		Synchronize<void>::process(this_->criticalSection, [this_]() {
			// not atomic, protected only by the critical section
			int value = this_->value;
			++value;
			this_->value = value;
		});
	};
};

void lockIncrementProcedure(void *this__) {
	Counter *this_ = reinterpret_cast<Counter *>(this__);
	int k;
	for (k = 0; k < increments; ++k) {
		CriticalSectionLock lock(this_->criticalSection);
		int value = this_->value;
		++value;
		this_->value = value;
	};
};

bool runCounter(ThreadProcedure procedure) {
	const int threadCount = 4;
	Counter counter;
	Thread threads[threadCount];
	int k;

	counter.value = 0;
	for (k = 0; k < threadCount; ++k) {
		threads[k].start(procedure, &counter);
	};
	for (k = 0; k < threadCount; ++k) {
		threads[k].join();
	};
	return counter.value == threadCount * increments;
};

void testMutualExclusion() {
	check(runCounter(incrementProcedure), "Synchronize: 4 threads x 10000 increments");
	check(runCounter(lockIncrementProcedure), "CriticalSectionLock: 4 threads x 10000 increments");
};

// ---

void test() {
	Watchdog watchdog;
	Thread watchdogThread;

	// keep output if process crashes
	setvbuf(stdout, nullptr, _IONBF, 0);

	watchdog.done.set(false);
	watchdogThread.start(watchdogProcedure, &watchdog);

	printf("- CriticalSectionLock, Synchronize\r\n");
	testReturnTypes();
	testCallables();
	testException();
	testMutualExclusion();

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
