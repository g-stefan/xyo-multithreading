// Multithreading
// Copyright (c) 2016-2025 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2025 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/WorkerQueue.hpp>

namespace XYO::Multithreading {

	WorkerQueue::WorkerQueue() {
		numberOfThreads = Processor::getCount();
		allDone = false;
	};

	WorkerQueue::~WorkerQueue() {
		process();
	};

	void WorkerQueue::add(WorkerProcedure workerProcedure_,
	                      TransferProcedure transferReturnValue_,
	                      TransferProcedure transferParameter_,
	                      Object *parameter) {
		WorkerQueueNode &node(queue.index(queue.length()));
#ifdef XYO_PLATFORM_MULTI_THREAD
		node.worker.setProcedure(workerProcedure_);
		node.worker.setTransferParameter(transferParameter_);
		node.worker.setTransferReturnValue(transferReturnValue_);
#endif
#ifdef XYO_PLATFORM_SINGLE_THREAD
		node.workerProcedure = workerProcedure_;
#endif

		node.parameter = parameter;
	};

	void WorkerQueue::setNumberOfThreads(int numberOfThreads_) {
		numberOfThreads = numberOfThreads_;
		if (numberOfThreads == 0) {
			numberOfThreads = Processor::getCount();
		};
	};

	int WorkerQueue::getNumberOfThreads() {
		return numberOfThreads;
	};

	bool WorkerQueue::process() {
		size_t k;
		if (allDone) {
			return true;
		};
#ifdef XYO_PLATFORM_MULTI_THREAD

		for (;;) {
			size_t countDone = 0;
			for (k = 0; k < queue.length(); ++k) {
				WorkerQueueNode &node(queue.index(k));
				if (node.started) {
					if (!node.worker.isRunning()) {
						node.worker.endWork();
						++countDone;
					};
				};
			};
			if (countDone == queue.length()) {
				allDone = true;
				return true;
			};
			int countRunning = 0;
			for (k = 0; k < queue.length(); ++k) {
				WorkerQueueNode &node(queue.index(k));
				if (node.started) {
					if (node.worker.isRunning()) {
						++countRunning;
					};
				};
			};
			if (countRunning < numberOfThreads) {
				int count = numberOfThreads - countRunning;
				for (k = 0; (k < queue.length()) && (count > 0); ++k) {
					WorkerQueueNode &node(queue.index(k));
					if (!node.started) {
						node.started = node.worker.start(node.parameter);
						if (!node.started) {
							return false;
						};
						--count;
					};
				};
			};
			Thread::sleep(1);
		};

		return false;
#endif
#ifdef XYO_PLATFORM_SINGLE_THREAD
		TAtomic<bool> requestToTerminate;
		for (k = 0; k < queue.length(); ++k) {
			WorkerQueueNode &node(queue.index(k));
			requestToTerminate.set(false);
			node.returnValue = (*node.workerProcedure)(node.parameter, requestToTerminate);
		};
#endif
		allDone = true;
		return true;
	};

	TPointer<Object> WorkerQueue::getReturnValue(size_t index) {
#ifdef XYO_PLATFORM_MULTI_THREAD
		return (queue.index(index)).worker.getReturnValue();
#endif
#ifdef XYO_PLATFORM_SINGLE_THREAD
		return (queue.index(index)).returnValue;
#endif
	};

	void WorkerQueue::setParameter(size_t index, Object *parameter) {
		(queue.index(index)).parameter = parameter;
	};

	void WorkerQueue::reset() {
		queue.empty();
		allDone = false;
	};

	WorkerQueueNode &WorkerQueue::index(size_t index) {
		return queue.index(index);
	};

};
