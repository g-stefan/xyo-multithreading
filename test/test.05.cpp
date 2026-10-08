// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>
#include <cstdlib>
#include <chrono>
#include <thread>

using namespace XYO::Multithreading;

// Worker and WorkerQueue lifecycle
//
// - Worker can be started again after endWork()
// - Worker without procedure does not start
// - WorkerQueue number of threads less than 1 uses number of processors
// - WorkerQueue can be reused after reset()
// - Worker / WorkerQueue node recycled (activeDestructor) keeps no result
// - WorkerQueue destructor waits for started work to end normally,
//   work not started is not done
// - Worker endWork() / destructor / activeDestructor() while work is
//   running: requests termination, waits for the work, no crash
// - Worker hand over is not polling: 1000 works in less than 1000 ms
// - WorkerQueue::process() is not polling: 1000 works on 1 thread in
//   less than 1000 ms
// - WorkerQueue thread pool: threads reused, no thread per work

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

struct Value : public Object {
		int value;
};

TPointer<Value> newValue(int value) {
	TPointer<Value> retV;
	retV.newMemory();
	retV->value = value;
	return retV;
};

TPointer<Value> transferValue(Value &value) {
	return newValue(value.value);
};

// negative value - failed work
TPointer<Value> processProcedure(Value *parameter, TAtomic<bool> &requestToTerminate) {
	if (parameter->value < 0) {
		throw std::runtime_error("negative value");
	};
	return newValue(parameter->value * 2);
};

typedef TWorker<Value, Value, transferValue, transferValue, processProcedure> ProcessWorker;
typedef TWorkerQueue<Value, Value, transferValue, transferValue, processProcedure> ProcessQueue;

TAtomic<int> slowStarted;
TAtomic<int> slowCompleted;
TAtomic<bool> slowRequestToTerminate;

// about 200 ms, records if termination was requested
TPointer<Value> slowProcedure(Value *parameter, TAtomic<bool> &requestToTerminate) {
	int k;
	slowStarted.fetchAdd(1);
	for (k = 0; k < 20; ++k) {
		if (requestToTerminate.get()) {
			slowRequestToTerminate.set(true);
		};
		Thread::sleep(10);
	};
	slowCompleted.fetchAdd(1);
	return newValue(parameter->value * 2);
};

typedef TWorker<Value, Value, transferValue, transferValue, slowProcedure> SlowWorker;
typedef TWorkerQueue<Value, Value, transferValue, transferValue, slowProcedure> SlowQueue;

void resetSlowCount() {
	slowStarted.set(0);
	slowCompleted.set(0);
	slowRequestToTerminate.set(false);
};

bool runWorker(Worker &worker, int value) {
	if (!ProcessWorker::start(worker, newValue(value))) {
		return false;
	};
	worker.join();
	TPointer<Value> returnValue = ProcessWorker::getReturnValue(worker);
	return (returnValue) && (returnValue->value == value * 2);
};

void testWorkerRestart() {
	Worker worker;

	ProcessWorker::set(worker);
	check(runWorker(worker, 21), "worker: first work");
	worker.endWork();
	check(runWorker(worker, 5), "worker: work after endWork");
	worker.endWork();
	worker.endWork();
	check(runWorker(worker, 7), "worker: work after endWork twice");
};

void testWorkerRecycle() {
	Worker worker;

	ProcessWorker::set(worker);
	check(runWorker(worker, 21), "worker recycle: work");
	worker.activeDestructor();
	check(!worker.getReturnValue() && !worker.hasFailed(), "worker recycle: no result");
	check(!worker.start(nullptr), "worker recycle: no procedure");

	ProcessWorker::set(worker);
	check(!runWorker(worker, -1) && worker.hasFailed(), "worker recycle: failed work");
	worker.activeDestructor();
	check(!worker.hasFailed(), "worker recycle: failed state cleared");
};

void testWorkerWithoutProcedure() {
	Worker worker;

	check(!worker.start(nullptr), "worker without procedure: start returns false");
	worker.endWork();
	check(!worker.isRunning(), "worker without procedure: not running");
};

// ---

// A WorkerQueue that loops forever must fail the test, not hang it.
// process() must run in this thread (return values are allocated by the
// thread that calls it), a watchdog thread ends the process on time limit.

struct Watchdog {
		const char *name;
		TAtomic<bool> done;
};

void watchdogProcedure(void *this__) {
	const int timeLimit = 10000;
	Watchdog *this_ = reinterpret_cast<Watchdog *>(this__);
	int k;

	for (k = 0; k < timeLimit; k += 10) {
		if (this_->done.get()) {
			return;
		};
		Thread::sleep(10);
	};

	printf("[FAIL] %s: process() did not end in %d ms\r\n", this_->name, timeLimit);
	printf("* Error: test failed\n");
	fflush(stdout);
	std::_Exit(1);
};

bool processWithTimeLimit(WorkerQueue &queue, const char *name) {
	Watchdog watchdog;
	Thread thread;
	bool retV;

	watchdog.name = name;
	watchdog.done.set(false);
	if (!thread.start(watchdogProcedure, &watchdog)) {
		return false;
	};
	retV = queue.process();
	watchdog.done.set(true);
	thread.join();
	return retV;
};

bool checkQueueResult(WorkerQueue &queue, size_t count) {
	size_t k;
	if (queue.length() != count) {
		return false;
	};
	for (k = 0; k < count; ++k) {
		TPointer<Value> returnValue = TStaticCast<Value *>(queue.getReturnValue(k));
		if (!returnValue || (returnValue->value != (int)k * 2)) {
			return false;
		};
	};
	return true;
};

void testNumberOfThreads() {
	char message[256];
	int numberOfThreads;
	size_t k;

	{
		WorkerQueue queue;
		queue.setNumberOfThreads(3);
		check(queue.getNumberOfThreads() == 3, "worker queue: set 3 threads");
		queue.setNumberOfThreads(0);
		check(queue.getNumberOfThreads() == Processor::getCount(), "worker queue: set 0 threads uses number of processors");
	};

	numberOfThreads = -1;
	while (numberOfThreads >= -2) {
		WorkerQueue queue;
		queue.setNumberOfThreads(numberOfThreads);
		sprintf(message, "worker queue: set %d threads, get %d, processors %d", numberOfThreads, queue.getNumberOfThreads(), Processor::getCount());
		check(queue.getNumberOfThreads() == Processor::getCount(), message);

		for (k = 0; k < 16; ++k) {
			ProcessQueue::add(queue, newValue((int)k));
		};
		sprintf(message, "worker queue: set %d threads, process", numberOfThreads);
		check(processWithTimeLimit(queue, message) && checkQueueResult(queue, 16), message);

		--numberOfThreads;
	};
};

void testQueueReset() {
	WorkerQueue queue;
	size_t k;

	queue.setNumberOfThreads(2);
	for (k = 0; k < 8; ++k) {
		ProcessQueue::add(queue, newValue((int)k));
	};
	check(processWithTimeLimit(queue, "worker queue: first process") && checkQueueResult(queue, 8), "worker queue: first process");

	queue.reset();
	for (k = 0; k < 5; ++k) {
		ProcessQueue::add(queue, newValue((int)k));
	};
	check(processWithTimeLimit(queue, "worker queue: process after reset") && checkQueueResult(queue, 5), "worker queue: process after reset");

	// recycled nodes must not keep result or failed state of previous process
	queue.reset();
	ProcessQueue::add(queue, newValue(-1));
	ProcessQueue::add(queue, newValue(1));
	processWithTimeLimit(queue, "worker queue: process with failed work");
	check(queue.hasFailed(0) && queue.getReturnValue(1), "worker queue: node 0 failed, node 1 has result");
	queue.reset();
	ProcessQueue::add(queue, newValue(0));
	ProcessQueue::add(queue, newValue(1));
	check(!queue.hasFailed(0) && !queue.getReturnValue(0) && !queue.getReturnValue(1), "worker queue: after reset no result or failed state of previous process");
	check(processWithTimeLimit(queue, "worker queue: process after failed work") && checkQueueResult(queue, 2), "worker queue: process after failed work");

	// node without procedure, recycled node must not keep old procedures
	queue.reset();
	queue.index(0);
	check(!processWithTimeLimit(queue, "worker queue: node without procedure"), "worker queue: node without procedure, process returns false");
	queue.reset();
};

void testQueueDestructor() {
	char message[256];
	int k;

	resetSlowCount();
	{
		WorkerQueue queue;
		for (k = 0; k < 4; ++k) {
			SlowQueue::add(queue, newValue(k));
		};
	};
	sprintf(message, "worker queue destructor, process() not called: work started %d", slowStarted.get());
	check(slowStarted.get() == 0, message);

	resetSlowCount();
	{
		WorkerQueue queue;
		queue.setNumberOfThreads(2);
		// work 0 starts on thread 0, work 1 has no procedure: process()
		// returns false while work 0 is running, works 2 and 3 not started
		SlowQueue::add(queue, newValue(0));
		queue.index(1);
		SlowQueue::add(queue, newValue(2));
		SlowQueue::add(queue, newValue(3));
		check(!queue.process(), "worker queue destructor: process() fails at work 1, work 0 running");
	};
	sprintf(message, "worker queue destructor, one work started: started %d, completed %d, termination requested %s",
	        slowStarted.get(), slowCompleted.get(), slowRequestToTerminate.get() ? "yes" : "no");
	check((slowStarted.get() == 1) && (slowCompleted.get() == 1) && !slowRequestToTerminate.get(), message);
};

bool isSlowWorkEnded(const char *name) {
	char message[256];
	bool isOk = (slowStarted.get() == 1) && (slowCompleted.get() == 1) && slowRequestToTerminate.get();
	sprintf(message, "%s: started %d, completed %d, termination requested %s",
	        name, slowStarted.get(), slowCompleted.get(), slowRequestToTerminate.get() ? "yes" : "no");
	check(isOk, message);
	return isOk;
};

void testEndWhileRunning() {
	bool isOk;
	int k;

	resetSlowCount();
	{
		Worker worker;
		SlowWorker::set(worker);
		SlowWorker::start(worker, newValue(1));
		worker.endWork();
	};
	isSlowWorkEnded("worker endWork() while work running");

	resetSlowCount();
	{
		Worker worker;
		SlowWorker::set(worker);
		SlowWorker::start(worker, newValue(1));
		worker.activeDestructor();
	};
	isSlowWorkEnded("worker activeDestructor() while work running");

	resetSlowCount();
	{
		Worker worker;
		SlowWorker::set(worker);
		SlowWorker::start(worker, newValue(1));
	};
	isSlowWorkEnded("worker destructor while work running");

	// worker thread ends right after endWork(), owner must not read it
	isOk = true;
	for (k = 0; k < 100; ++k) {
		Worker worker;
		ProcessWorker::set(worker);
		if (!runWorker(worker, k)) {
			isOk = false;
		};
		worker.endWork();
	};
	check(isOk, "worker endWork() right after work, 100 rounds");
};

void testWorkerSpeed() {
	const int count = 1000;
	Worker worker;
	std::chrono::steady_clock::time_point begin;
	char message[256];
	bool isOk = true;
	int time;
	int k;

	ProcessWorker::set(worker);
	begin = std::chrono::steady_clock::now();
	for (k = 0; k < count; ++k) {
		if (!runWorker(worker, k)) {
			isOk = false;
		};
	};
	time = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();

	// polling with sleep(1) needs at least 1 ms per wait, several waits per work
	sprintf(message, "worker: %d works one after another in %d ms, no polling", count, time);
	check(isOk && (time < count), message);
};

void testQueueSpeed() {
	const size_t count = 1000;
	std::chrono::steady_clock::time_point begin;
	char message[256];
	int numberOfThreads;
	bool isOk;
	int time;
	size_t k;

	for (numberOfThreads = 1; numberOfThreads <= 4; numberOfThreads += 3) {
		WorkerQueue queue;
		queue.setNumberOfThreads(numberOfThreads);
		for (k = 0; k < count; ++k) {
			ProcessQueue::add(queue, newValue((int)k));
		};
		begin = std::chrono::steady_clock::now();
		isOk = processWithTimeLimit(queue, "worker queue speed");
		time = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
		isOk = isOk && checkQueueResult(queue, count);

		// polling with sleep(1) needs at least 1 ms per work on 1 thread
		sprintf(message, "worker queue: %zu works on %d thread(s) in %d ms, no polling", count, numberOfThreads, time);
		check(isOk && ((numberOfThreads > 1) || (time < (int)count)), message);
	};
};

// return value is the id of the thread running the work
TPointer<Value> threadIdProcedure(Value *parameter, TAtomic<bool> &requestToTerminate) {
	return newValue((int)std::hash<std::thread::id>()(std::this_thread::get_id()));
};

typedef TWorkerQueue<Value, Value, transferValue, transferValue, threadIdProcedure> ThreadIdQueue;

void testQueuePool() {
	const int numberOfThreads = 4;
	const size_t count = 200;
	int threadIds[count];
	char message[256];
	int distinct;
	size_t k;
	size_t m;

	{
		WorkerQueue queue;
		queue.setNumberOfThreads(numberOfThreads);
		for (k = 0; k < count; ++k) {
			ThreadIdQueue::add(queue, newValue((int)k));
		};
		check(processWithTimeLimit(queue, "worker queue pool"), "worker queue pool: process");
		distinct = 0;
		for (k = 0; k < count; ++k) {
			TPointer<Value> returnValue = TStaticCast<Value *>(queue.getReturnValue(k));
			threadIds[k] = returnValue ? returnValue->value : 0;
			for (m = 0; m < k; ++m) {
				if (threadIds[m] == threadIds[k]) {
					break;
				};
			};
			if (m == k) {
				++distinct;
			};
		};
	};
	sprintf(message, "worker queue pool: %zu works run by %d thread(s), at most %d", count, distinct, numberOfThreads);
	check((distinct >= 1) && (distinct <= numberOfThreads), message);

	// Thread reuse is checked above by the number of threads. The time
	// depends on the OS wake up time (about 5 wake ups per work: Windows
	// about 15 ms, Linux on WSL2 about 150 ms for 1000 works), checked only
	// against polling (at least 1 ms per work)
	{
		const size_t countFast = 1000;
		WorkerQueue queue;
		std::chrono::steady_clock::time_point begin;
		int time;
		queue.setNumberOfThreads(1);
		for (k = 0; k < countFast; ++k) {
			ProcessQueue::add(queue, newValue((int)k));
		};
		begin = std::chrono::steady_clock::now();
		bool isOk = processWithTimeLimit(queue, "worker queue pool speed");
		time = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
		isOk = isOk && checkQueueResult(queue, countFast);
		sprintf(message, "worker queue pool: %zu works on 1 thread in %d ms, no polling", countFast, time);
		check(isOk && (time < (int)countFast), message);
	};
};

void test() {
	// keep output if process crashes
	setvbuf(stdout, nullptr, _IONBF, 0);

	printf("- Worker\r\n");
	testWorkerRestart();
	testWorkerRecycle();
	testWorkerWithoutProcedure();
	testEndWhileRunning();
	testWorkerSpeed();

	printf("- WorkerQueue\r\n");
	testNumberOfThreads();
	testQueueReset();
	testQueueDestructor();
	testQueueSpeed();
	testQueuePool();

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
