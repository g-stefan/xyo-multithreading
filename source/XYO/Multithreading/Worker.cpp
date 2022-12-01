// Multithreading
// Copyright (c) 2022 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2022 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Worker.hpp>

#ifdef XYO_MULTI_THREAD

namespace XYO::Multithreading {

	class Worker_ {
		public:
			Thread thread;
			Transfer transfer;
			WorkerProcedure workerProcedure;
			TransferProcedure transferParameter;
			TransferProcedure transferReturnValue;
			TAtomic<bool> requestToTerminateSuper;
			TAtomic<bool> requestToTerminateWorker;
			TPointer<Object> returnValue;
			TAtomic<bool> workEnd;
			TAtomic<bool> startedOk;
	};

	Worker::Worker() {
		worker = new Worker_();
		worker->workerProcedure = nullptr;
		worker->transferParameter = nullptr;
		worker->transferReturnValue = nullptr;
		worker->requestToTerminateSuper.set(false);
		worker->requestToTerminateWorker.set(false);
		worker->workEnd.set(true);
		worker->startedOk.set(false);
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

	static void workerThreadProcedure(void *this__) {
		Worker_ *worker = reinterpret_cast<Worker_ *>(this__);
		Transfer transfer;
		transfer.link(&worker->transfer);
		TPointer<Object> parameter;
		worker->startedOk.set(true);
		while (!worker->requestToTerminateSuper.get()) {
			if (transfer.hasValue()) {
				worker->workEnd.set(false);
				parameter = transfer.get(worker->transferParameter);
				transfer.set((*worker->workerProcedure)(parameter, worker->requestToTerminateWorker));
				worker->requestToTerminateWorker.set(false);
				worker->workEnd.set(true);
				continue;
			};
			Thread::sleep(1);
		};
	};

	bool Worker::beginWork() {
		if (worker->thread.start(workerThreadProcedure, worker)) {
			while (!worker->startedOk.get()) {
				Thread::sleep(1);
			};
			return true;
		};
		return false;
	};

	void Worker::endWork() {
		worker->requestToTerminateWorker.set(true);
		worker->requestToTerminateSuper.set(true);
		join();
		worker->thread.join();
		worker->workerProcedure = nullptr;
		worker->transferParameter = nullptr;
		worker->transferReturnValue = nullptr;
		worker->requestToTerminateSuper.set(false);
		worker->requestToTerminateWorker.set(false);
		worker->workEnd.set(true);
		worker->startedOk.set(false);
		worker->transfer.link(nullptr);
	};

	bool Worker::start(Object *parameter) {
		if (!worker->thread.isRunning()) {
			if (!beginWork()) {
				return false;
			};
		};
		join();
		worker->returnValue.deleteMemory();
		worker->transfer.set(parameter);
		return true;
	};

	void Worker::join() {
		while (isRunning()) {
			Thread::sleep(1);
		};
	};

	bool Worker::isRunning() {
		if (worker->transfer.hasValue()) {
			worker->returnValue = worker->transfer.get(worker->transferReturnValue);
		};
		return (!worker->workEnd.get());
	};

	void Worker::requestToTerminate() {
		worker->requestToTerminateWorker.set(true);
	};

	TPointer<Object> Worker::getReturnValue() {
		if (worker->transfer.hasValue()) {
			worker->returnValue = worker->transfer.get(worker->transferReturnValue);
		};
		return worker->returnValue;
	};

};

#endif
