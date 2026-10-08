// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Worker.hpp>

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	class Worker_ {
		public:
			Thread thread;
			// owner side
			Transfer transfer;
			// worker thread side, linked to transfer; kept here, not on the
			// worker thread stack, transfer reads it after the thread ends
			Transfer transferThread;
			WorkerProcedure workerProcedure;
			TransferProcedure transferParameter;
			TransferProcedure transferReturnValue;
			TAtomic<bool> requestToTerminateSuper;
			TAtomic<bool> requestToTerminateWorker;
			TPointer<Object> returnValue;
			TAtomic<bool> workEnd;
			// worker thread is running and linked, notified by the worker thread
			Semaphore threadStarted;
			TAtomic<bool> failed;
	};

	Worker::Worker() {
		worker = new Worker_();
		worker->workerProcedure = nullptr;
		worker->transferParameter = nullptr;
		worker->transferReturnValue = nullptr;
		worker->requestToTerminateSuper.set(false);
		worker->requestToTerminateWorker.set(false);
		worker->workEnd.set(true);
		worker->failed.set(false);
	};

	Worker::~Worker() {
		endWork();
		delete worker;
	};

	void Worker::setProcedure(WorkerProcedure workerProcedure_) {
		worker->workerProcedure = workerProcedure_;
	};

	void Worker::setTransferParameter(TransferProcedure transferParameter_) {
		worker->transferParameter = transferParameter_;
	};

	void Worker::setTransferReturnValue(TransferProcedure transferReturnValue_) {
		worker->transferReturnValue = transferReturnValue_;
	};

	void Worker::setNotify(Semaphore *notify) {
		worker->transfer.setNotify(notify);
	};

	static void workerThreadProcedure(void *this__) {
		Worker_ *worker = reinterpret_cast<Worker_ *>(this__);
		Transfer &transfer(worker->transferThread);
		transfer.link(nullptr);
		transfer.link(&worker->transfer);
		TPointer<Object> parameter;
		worker->threadStarted.notify();
		// sleeps in waitValue() until the owner posts work (set())
		// or requests to terminate (endWork(), notifyPeer())
		while (!worker->requestToTerminateSuper.get()) {
			if (transfer.hasValue()) {
				worker->workEnd.set(false);
				// A failed job has no return value, the thread must survive
				// to mark workEnd, otherwise join() will wait forever
				try {
					parameter = transfer.get(worker->transferParameter);
					transfer.set((*worker->workerProcedure)(parameter, worker->requestToTerminateWorker));
				} catch (...) {
					worker->failed.set(true);
				};
				worker->requestToTerminateWorker.set(false);
				worker->workEnd.set(true);
				// wake the owner from join(), also when there is no result
				transfer.notifyPeer();
				continue;
			};
			transfer.waitValue();
		};
	};

	bool Worker::beginWork() {
		if (worker->thread.start(workerThreadProcedure, worker)) {
			worker->threadStarted.wait();
			return true;
		};
		return false;
	};

	// Stop worker thread, keep procedures, start() can be used again
	void Worker::endWork() {
		worker->requestToTerminateWorker.set(true);
		worker->requestToTerminateSuper.set(true);
		// wake the worker thread if it waits for work, to see the request
		worker->transfer.notifyPeer();
		join();
		worker->thread.join();
		worker->requestToTerminateSuper.set(false);
		worker->requestToTerminateWorker.set(false);
		worker->workEnd.set(true);
		worker->threadStarted.reset();
		worker->transfer.link(nullptr);
	};

	bool Worker::start(Object *parameter) {
		if (!worker->workerProcedure) {
			return false;
		};
		if (!worker->thread.isRunning()) {
			if (!beginWork()) {
				return false;
			};
		};
		join();
		worker->returnValue.deleteMemory();
		worker->failed.set(false);
		worker->transfer.set(parameter);
		return true;
	};

	// Woken by the worker thread when the result is posted and when the
	// work ends; isRunning() takes the result
	void Worker::join() {
		while (isRunning()) {
			worker->transfer.waitValue();
		};
	};

	// Called from endWork() and so from destructors, must not throw,
	// a failed return value transfer is reported as no return value
	static void fetchReturnValue(Worker_ *worker) {
		if (worker->transfer.hasValue()) {
			try {
				worker->returnValue = worker->transfer.get(worker->transferReturnValue);
			} catch (...) {
				worker->returnValue.deleteMemory();
				worker->failed.set(true);
			};
		};
	};

	bool Worker::isRunning() {
		fetchReturnValue(worker);
		return (!worker->workEnd.get());
	};

	void Worker::requestToTerminate() {
		worker->requestToTerminateWorker.set(true);
	};

	TPointer<Object> Worker::getReturnValue() {
		fetchReturnValue(worker);
		return worker->returnValue;
	};

	bool Worker::hasFailed() {
		fetchReturnValue(worker);
		return worker->failed.get();
	};

	// endWork() keeps procedures and last result,
	// a recycled Worker must not keep them
	void Worker::activeDestructor() {
		endWork();
		worker->workerProcedure = nullptr;
		worker->transferParameter = nullptr;
		worker->transferReturnValue = nullptr;
		worker->transfer.setNotify(nullptr);
		worker->returnValue.deleteMemory();
		worker->failed.set(false);
	};

};

#endif
