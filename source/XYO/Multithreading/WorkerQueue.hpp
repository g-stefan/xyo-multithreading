// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_WORKERQUEUE_HPP
#define XYO_MULTITHREADING_WORKERQUEUE_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifndef XYO_MULTITHREADING_WORKER_HPP
#	include <XYO/Multithreading/Worker.hpp>
#endif

namespace XYO::Multithreading {

	// A work of the queue, its result is kept here after the work ends
	class WorkerQueueNode : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(WorkerQueueNode);

		public:
			WorkerProcedure workerProcedure;
#ifdef XYO_PLATFORM_MULTI_THREAD
			TransferProcedure transferParameter;
			TransferProcedure transferReturnValue;
#endif
			TPointer<Object> parameter;
			TPointer<Object> returnValue;
			// started - given to a thread, done - ended,
			// failed - thrown an exception (no return value)
			bool started;
			bool done;
			bool failed;

			inline WorkerQueueNode() {
				workerProcedure = nullptr;
#ifdef XYO_PLATFORM_MULTI_THREAD
				transferParameter = nullptr;
				transferReturnValue = nullptr;
#endif
				started = false;
				done = false;
				failed = false;
			};

			inline void activeDestructor() {
				workerProcedure = nullptr;
#ifdef XYO_PLATFORM_MULTI_THREAD
				transferParameter = nullptr;
				transferReturnValue = nullptr;
#endif
				parameter.deleteMemory();
				returnValue.deleteMemory();
				started = false;
				done = false;
				failed = false;
			};
	};

#ifdef XYO_PLATFORM_MULTI_THREAD

	// A thread of the WorkerQueue pool, runs one work at a time
	class WorkerQueueThread : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(WorkerQueueThread);

		public:
			Worker worker;
			size_t node;
			bool isBusy;

			inline WorkerQueueThread() {
				node = 0;
				isBusy = false;
			};

			inline void activeDestructor() {
				worker.activeDestructor();
				node = 0;
				isBusy = false;
			};
	};

#endif

	//
	// Run the added work on a pool of up to numberOfThreads threads, on
	// process(). Each thread runs one work after another, threads are
	// started by process() and ended when all work is done.
	// The destructor waits for started work to end, work not started
	// (process() not called or failed) is not done.
	// Return value and failed state of each work: getReturnValue(index),
	// hasFailed(index).
	//
	class WorkerQueue : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(WorkerQueue);

		protected:
			int numberOfThreads;
#ifdef XYO_PLATFORM_MULTI_THREAD
			// notified by the pool threads when a return value is posted or a
			// work ends, process() waits on it; declared before pool,
			// destroyed after the threads
			Semaphore workerSignal;
			TDynamicArray<WorkerQueueThread> pool;
#endif
			TDynamicArray<WorkerQueueNode> queue;
			// next work to start
			size_t nextNode;
			bool allDone;

#ifdef XYO_PLATFORM_MULTI_THREAD
			void endPool();
#endif

		public:
			XYO_MULTITHREADING_EXPORT WorkerQueue();
			XYO_MULTITHREADING_EXPORT ~WorkerQueue();
			XYO_MULTITHREADING_EXPORT void add(WorkerProcedure workerProcedure_,
			                                   TransferProcedure transferReturnValue_,
			                                   TransferProcedure transferParameter_,
			                                   Object *parameter);
			XYO_MULTITHREADING_EXPORT void setNumberOfThreads(int numberOfThreads_);
			XYO_MULTITHREADING_EXPORT int getNumberOfThreads();
			XYO_MULTITHREADING_EXPORT bool process();
			XYO_MULTITHREADING_EXPORT TPointer<Object> getReturnValue(size_t index);
			XYO_MULTITHREADING_EXPORT bool hasFailed(size_t index);
			XYO_MULTITHREADING_EXPORT void setParameter(size_t index, Object *parameter);
			XYO_MULTITHREADING_EXPORT void reset();
			XYO_MULTITHREADING_EXPORT WorkerQueueNode &index(size_t index);

			size_t length() const {
				return queue.length();
			};

			bool isEmpty() const {
				return queue.isEmpty();
			};
	};

	template <typename ReturnT,
	          typename ParameterT,
	          TPointer<ReturnT> TransferReturnT(ReturnT &),
	          TPointer<ParameterT> TransferParameterT(ParameterT &),
	          TPointer<ReturnT> WorkerProcedureT(ParameterT *, TAtomic<bool> &)>
	struct TWorkerQueue {

			static inline void add(WorkerQueue &workerQueue, ParameterT *parameter) {
				workerQueue.add(
				    TGetWorkerProcedure<ReturnT, ParameterT, WorkerProcedureT>::workerProcedure,
				    TGetTransferProcedure<ReturnT, TransferReturnT>::transferProcedure,
				    TGetTransferProcedure<ParameterT, TransferParameterT>::transferProcedure,
				    TStaticCast<Object *>(parameter));
			};
	};

};

#endif
