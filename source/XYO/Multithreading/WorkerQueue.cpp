// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/WorkerQueue.hpp>

namespace XYO::Multithreading {

	WorkerQueue::WorkerQueue() {
		numberOfThreads = Processor::getCount();
		nextNode = 0;
		allDone = false;
	};

	WorkerQueue::~WorkerQueue() {
#ifdef XYO_PLATFORM_MULTI_THREAD
		// wait for started work to end normally, before the pool is
		// destroyed (endWork() would request termination), work not
		// started is not done
		size_t k;
		for (k = 0; k < pool.length(); ++k) {
			WorkerQueueThread &thread(pool.index(k));
			if (thread.isBusy) {
				thread.worker.join();
			};
		};
#endif
	};

	void WorkerQueue::add(WorkerProcedure workerProcedure_,
	                      TransferProcedure transferReturnValue_,
	                      TransferProcedure transferParameter_,
	                      Object *parameter) {
		WorkerQueueNode &node(queue.index(queue.length()));
		node.workerProcedure = workerProcedure_;
#ifdef XYO_PLATFORM_MULTI_THREAD
		node.transferParameter = transferParameter_;
		node.transferReturnValue = transferReturnValue_;
#endif
		node.parameter = parameter;
	};

	// 0 or less - use number of processors
	void WorkerQueue::setNumberOfThreads(int numberOfThreads_) {
		numberOfThreads = numberOfThreads_;
		if (numberOfThreads < 1) {
			numberOfThreads = Processor::getCount();
		};
	};

	int WorkerQueue::getNumberOfThreads() {
		return numberOfThreads;
	};

#ifdef XYO_PLATFORM_MULTI_THREAD

	// Stop the pool threads, a running work is requested to terminate
	void WorkerQueue::endPool() {
		size_t k;
		for (k = 0; k < pool.length(); ++k) {
			WorkerQueueThread &thread(pool.index(k));
			thread.worker.endWork();
			thread.isBusy = false;
		};
	};

	bool WorkerQueue::process() {
		size_t k;
		if (allDone) {
			return true;
		};
		if (nextNode >= queue.length()) {
			allDone = true;
			return true;
		};

		size_t poolLength = (size_t)numberOfThreads;
		if (poolLength > queue.length()) {
			poolLength = queue.length();
		};
		if (pool.length() < poolLength) {
			pool.index(poolLength - 1);
		};

		for (;;) {
			bool isBusy = false;

			// ended work: keep result in the node, taken in this thread
			for (k = 0; k < pool.length(); ++k) {
				WorkerQueueThread &thread(pool.index(k));
				if (!thread.isBusy) {
					continue;
				};
				if (thread.worker.isRunning()) {
					isBusy = true;
					continue;
				};
				WorkerQueueNode &node(queue.index(thread.node));
				node.returnValue = thread.worker.getReturnValue();
				node.failed = thread.worker.hasFailed();
				node.done = true;
				thread.isBusy = false;
			};

			// next work on free threads, the thread is started by the first work
			for (k = 0; (k < poolLength) && (nextNode < queue.length()); ++k) {
				WorkerQueueThread &thread(pool.index(k));
				if (thread.isBusy) {
					continue;
				};
				WorkerQueueNode &node(queue.index(nextNode));
				thread.worker.setProcedure(node.workerProcedure);
				thread.worker.setTransferParameter(node.transferParameter);
				thread.worker.setTransferReturnValue(node.transferReturnValue);
				thread.worker.setNotify(&workerSignal);
				if (!thread.worker.start(node.parameter)) {
					return false;
				};
				node.started = true;
				thread.node = nextNode;
				thread.isBusy = true;
				isBusy = true;
				++nextNode;
			};

			if ((!isBusy) && (nextNode >= queue.length())) {
				endPool();
				allDone = true;
				return true;
			};

			// sleep until a thread posts its return value or ends its work,
			// a notify during the scan above is kept, not lost
			workerSignal.wait();
		};
	};

#endif

#ifdef XYO_PLATFORM_SINGLE_THREAD

	bool WorkerQueue::process() {
		TAtomic<bool> requestToTerminate;
		if (allDone) {
			return true;
		};
		for (; nextNode < queue.length(); ++nextNode) {
			WorkerQueueNode &node(queue.index(nextNode));
			if (!node.workerProcedure) {
				return false;
			};
			node.started = true;
			requestToTerminate.set(false);
			try {
				node.returnValue = (*node.workerProcedure)(node.parameter, requestToTerminate);
			} catch (...) {
				node.returnValue.deleteMemory();
				node.failed = true;
			};
			node.done = true;
		};
		allDone = true;
		return true;
	};

#endif

	TPointer<Object> WorkerQueue::getReturnValue(size_t index) {
		if (index >= queue.length()) {
			return nullptr;
		};
		return (queue.index(index)).returnValue;
	};

	bool WorkerQueue::hasFailed(size_t index) {
		if (index >= queue.length()) {
			return false;
		};
		return (queue.index(index)).failed;
	};

	void WorkerQueue::setParameter(size_t index, Object *parameter) {
		if (index >= queue.length()) {
			return;
		};
		(queue.index(index)).parameter = parameter;
	};

	// A work still running (process() failed) is requested to terminate,
	// its result is not kept
	void WorkerQueue::reset() {
#ifdef XYO_PLATFORM_MULTI_THREAD
		endPool();
#endif
		queue.empty();
		nextNode = 0;
		allDone = false;
	};

	WorkerQueueNode &WorkerQueue::index(size_t index) {
		return queue.index(index);
	};

};
