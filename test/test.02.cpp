// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>

using namespace XYO::Multithreading;

// Exceptions thrown by a worker must not hang or crash the caller

enum {
	ModeOk,
	ModeThrowInProcedure,
	ModeThrowInTransferParameter,
	ModeThrowInTransferReturnValue
};

struct Value : public Object {
		int value;
		int mode;
};

TPointer<Value> newValue(int value, int mode) {
	TPointer<Value> retV;
	retV.newMemory();
	retV->value = value;
	retV->mode = mode;
	return retV;
};

TPointer<Value> transferParameter(Value &parameter) {
	if (parameter.mode == ModeThrowInTransferParameter) {
		throw std::runtime_error("transfer parameter");
	};
	return newValue(parameter.value, parameter.mode);
};

TPointer<Value> transferReturnValue(Value &returnValue) {
	if (returnValue.mode == ModeThrowInTransferReturnValue) {
		throw std::runtime_error("transfer return value");
	};
	return newValue(returnValue.value, returnValue.mode);
};

TPointer<Value> processProcedure(Value *parameter, TAtomic<bool> &requestToTerminate) {
	if (parameter->mode == ModeThrowInProcedure) {
		throw std::runtime_error("worker procedure");
	};
	return newValue(parameter->value * 2, parameter->mode);
};

typedef TWorker<Value, Value, transferReturnValue, transferParameter, processProcedure> ProcessWorker;
typedef TWorkerQueue<Value, Value, transferReturnValue, transferParameter, processProcedure> ProcessQueue;

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

bool isExpected(TPointer<Value> returnValue, int value, int mode) {
	if (mode == ModeOk) {
		return (returnValue) && (returnValue->value == value * 2);
	};
	return !returnValue;
};

void testWorker(int mode, const char *name) {
	Worker worker;
	TPointer<Value> returnValue;

	printf("- worker: %s\r\n", name);

	ProcessWorker::set(worker);

	check(ProcessWorker::start(worker, newValue(21, mode)), "start");
	worker.join();
	check(worker.hasFailed() == (mode != ModeOk), "hasFailed");
	check(isExpected(ProcessWorker::getReturnValue(worker), 21, mode), "return value");

	// worker thread must survive to process next work
	check(ProcessWorker::start(worker, newValue(5, ModeOk)), "start next work");
	worker.join();
	check(!worker.hasFailed(), "next work hasFailed reset");
	check(isExpected(ProcessWorker::getReturnValue(worker), 5, ModeOk), "next work return value");
};

void testWorkerQueue() {
	WorkerQueue queue;
	size_t k;

	printf("- worker queue\r\n");

	queue.setNumberOfThreads(2);
	for (k = 0; k < 8; ++k) {
		ProcessQueue::add(queue, newValue((int)k, (int)(k % 4)));
	};

	check(queue.process(), "process");

	bool isOk = true;
	for (k = 0; k < queue.length(); ++k) {
		int mode = (int)(k % 4);
		if (queue.hasFailed(k) != (mode != ModeOk)) {
			isOk = false;
		};
		if (!isExpected(TStaticCast<Value *>(queue.getReturnValue(k)), (int)k, mode)) {
			isOk = false;
		};
	};
	check(isOk, "hasFailed and return values");
};

void test() {
	testWorker(ModeOk, "no exception");
	testWorker(ModeThrowInProcedure, "exception in worker procedure");
	testWorker(ModeThrowInTransferParameter, "exception in transfer parameter");
	testWorker(ModeThrowInTransferReturnValue, "exception in transfer return value");
	testWorkerQueue();

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
