// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>
#include <thread>

using namespace XYO::Multithreading;

// Object reference count is not atomic, memory is managed per thread.
//
// 1. Worker / WorkerQueue - objects must be created and destroyed
//    in the same thread, none lost
// 2. Thread::onTimeout / setInterval / setIntervalActionFirst / onFinish
//    stress with no delay, they pass one object from parent to child
// 3. Passing a counted object to a thread:
//    a) child thread is the only owner, as used by Thread::onTimeout
//    b) parent and child both release it at the same time,
//       only with "test.03 race", can lose a release (leak)
//       or release twice (heap corruption, 0xC0000374):
//       dec [counter] / cmp [counter], 0 - both threads may read 0

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

// ---

TAtomic<int> valueConstructed;
TAtomic<int> valueDestroyed;
TAtomic<int> valueDestroyedInOtherThread;

struct Value : public Object {
		int value;
		std::thread::id ownerThread;

		Value() {
			value = 0;
			ownerThread = std::this_thread::get_id();
			valueConstructed.fetchAdd(1);
		};

		~Value() {
			if (ownerThread != std::this_thread::get_id()) {
				valueDestroyedInOtherThread.fetchAdd(1);
			};
			valueDestroyed.fetchAdd(1);
		};
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

TPointer<Value> processProcedure(Value *parameter, TAtomic<bool> &requestToTerminate) {
	return newValue(parameter->value * 2);
};

typedef TWorker<Value, Value, transferValue, transferValue, processProcedure> ProcessWorker;
typedef TWorkerQueue<Value, Value, transferValue, transferValue, processProcedure> ProcessQueue;

void resetValueCount() {
	valueConstructed.set(0);
	valueDestroyed.set(0);
	valueDestroyedInOtherThread.set(0);
};

void checkValueCount(const char *name) {
	char message[256];
	sprintf(message, "%s: constructed %d, destroyed %d, destroyed in other thread %d",
	        name,
	        valueConstructed.get(),
	        valueDestroyed.get(),
	        valueDestroyedInOtherThread.get());
	check((valueConstructed.get() == valueDestroyed.get()) && (valueDestroyedInOtherThread.get() == 0), message);
};

void testWorker() {
	const int count = 1000;
	int k;
	bool isOk = true;

	resetValueCount();
	{
		Worker worker;
		ProcessWorker::set(worker);
		for (k = 0; k < count; ++k) {
			ProcessWorker::start(worker, newValue(k));
			worker.join();
			TPointer<Value> returnValue = ProcessWorker::getReturnValue(worker);
			if (!returnValue || (returnValue->value != k * 2)) {
				isOk = false;
			};
		};
	};
	check(isOk, "worker: return values");
	checkValueCount("worker");
};

void testWorkerQueue() {
	const size_t count = 200;
	size_t k;
	bool isOk = true;

	resetValueCount();
	{
		WorkerQueue queue;
		queue.setNumberOfThreads(4);
		for (k = 0; k < count; ++k) {
			ProcessQueue::add(queue, newValue((int)k));
		};
		queue.process();
		for (k = 0; k < count; ++k) {
			TPointer<Value> returnValue = TStaticCast<Value *>(queue.getReturnValue(k));
			if (!returnValue || (returnValue->value != (int)k * 2)) {
				isOk = false;
			};
		};
	};
	check(isOk, "worker queue: return values");
	checkValueCount("worker queue");
};

// ---

const int threadBatch = 64;
TAtomic<int> callCount;

void countProcedure(void *) {
	callCount.fetchAdd(1);
};

void emptyProcedure(void *) {
};

void testOnTimeout() {
	const int count = 2000;
	TPointer<Thread> threads[threadBatch];
	bool isStarted = true;
	int k;

	callCount.set(0);
	for (k = 0; k < count; ++k) {
		// replacing the pointer joins the previous thread
		threads[k % threadBatch] = Thread::onTimeout(0, countProcedure, nullptr);
		if (!threads[k % threadBatch]) {
			isStarted = false;
		};
	};
	for (k = 0; k < threadBatch; ++k) {
		threads[k] = nullptr;
	};
	check(isStarted && (callCount.get() == count), "Thread::onTimeout");
};

void testSetInterval() {
	const int count = 2000;
	TPointer<Thread> threads[threadBatch];
	TAtomic<bool> clearInterval(true);
	bool isStarted = true;
	int k;

	// interval already cleared, child releases the shared object right away
	callCount.set(0);
	for (k = 0; k < count; ++k) {
		if (k & 1) {
			threads[k % threadBatch] = Thread::setInterval(clearInterval, 0, countProcedure, nullptr);
		} else {
			threads[k % threadBatch] = Thread::setIntervalActionFirst(clearInterval, 0, countProcedure, nullptr);
		};
		if (!threads[k % threadBatch]) {
			isStarted = false;
		};
	};
	for (k = 0; k < threadBatch; ++k) {
		threads[k] = nullptr;
	};
	check(isStarted && (callCount.get() == 0), "Thread::setInterval, Thread::setIntervalActionFirst");
};

void testOnFinish() {
	const int count = 500;
	bool isStarted = true;
	int k;

	callCount.set(0);
	for (k = 0; k < count; ++k) {
		Thread thread;
		if (!thread.start(emptyProcedure, nullptr)) {
			isStarted = false;
			continue;
		};
		TPointer<Thread> onFinish = Thread::onFinish(thread, countProcedure, nullptr);
		if (!onFinish) {
			isStarted = false;
			continue;
		};
		onFinish->join();
	};
	check(isStarted && (callCount.get() == count), "Thread::onFinish");
};

// ---

TAtomic<int> sharedConstructed;
TAtomic<int> sharedDestroyed;

struct Shared : public Object {
		TAtomic<bool> childReady;
		TAtomic<bool> release;

		Shared() {
			childReady.set(false);
			release.set(false);
			sharedConstructed.fetchAdd(1);
		};

		~Shared() {
			sharedDestroyed.fetchAdd(1);
		};
};

void ownedByThreadProcedure(void *this__) {
	TPointer<Shared> this_(reinterpret_cast<Shared *>(this__));
};

// Same steps as Thread::onTimeout
int ownedByThread(int count) {
	Thread threads[threadBatch];
	int k;

	sharedConstructed.set(0);
	sharedDestroyed.set(0);
	for (k = 0; k < count; ++k) {
		Shared *shared = TMemorySystem<Shared>::newMemory();
		if (!threads[k % threadBatch].start(ownedByThreadProcedure, shared)) {
			TMemorySystem<Shared>::deleteMemory(shared);
		};
		// shared must not be used here, thread is the owner
	};
	for (k = 0; k < threadBatch; ++k) {
		threads[k].join();
	};
	return sharedConstructed.get() - sharedDestroyed.get();
};

void releaseAtSignalProcedure(void *this__) {
	Shared *this_ = reinterpret_cast<Shared *>(this__);
	this_->childReady.set(true);
	while (!this_->release.get()) {
	};
	this_->decReferenceCount();
};

// Parent and child own the object, release at same time
int sharedRelease(int count) {
	Thread threads[threadBatch];
	int k;

	sharedConstructed.set(0);
	sharedDestroyed.set(0);
	for (k = 0; k < count; ++k) {
		TPointer<Shared> shared(TMemorySystem<Shared>::newMemory());
		shared->incReferenceCount();
		if (!threads[k % threadBatch].start(releaseAtSignalProcedure, shared)) {
			shared->decReferenceCount();
			continue;
		};
		while (!shared->childReady.get()) {
		};
		shared->release.set(true);
		// shared released here by parent, in parallel with child
	};
	for (k = 0; k < threadBatch; ++k) {
		threads[k].join();
	};
	return sharedConstructed.get() - sharedDestroyed.get();
};

void testSharedObjectPattern(bool race) {
	const int count = 10000;
	char message[256];
	int lost;

	lost = ownedByThread(count);
	sprintf(message, "object owned by thread, as Thread::onTimeout: %d of %d objects not released", lost, count);
	check(lost == 0, message);

	if (!race) {
		return;
	};

	// Informative, not counted as error, may crash the process
	lost = sharedRelease(count);
	printf("[INFO] shared object, parent and child release at same time: %d of %d objects not released\r\n", lost, count);
};

// ---

void test(bool race) {
	// keep output if process crashes
	setvbuf(stdout, nullptr, _IONBF, 0);

	printf("- per thread memory, Worker and WorkerQueue\r\n");
	testWorker();
	testWorkerQueue();

	printf("- Thread helpers, no delay\r\n");
	testOnTimeout();
	testSetInterval();
	testOnFinish();

	printf("- object passed from parent to child thread\r\n");
	testSharedObjectPattern(race);

	if (errorCount) {
		throw std::runtime_error("test failed");
	};

	printf("Done.\r\n");
};

int main(int cmdN, char *cmdS[]) {
	try {

		test((cmdN > 1) && (strcmp(cmdS[1], "race") == 0));

		return 0;

	} catch (const std::exception &e) {
		printf("* Error: %s\n", e.what());
	} catch (...) {
		printf("* Error: Unknown\n");
	};

	return 1;
};
