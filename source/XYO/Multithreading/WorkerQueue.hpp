// Multithreading
// Copyright (c) 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
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

	class WorkerQueueNode : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(WorkerQueueNode);

		public:
#ifdef XYO_PLATFORM_MULTI_THREAD
			Worker worker;
			bool started;
#endif
#ifdef XYO_PLATFORM_SINGLE_THREAD
			TPointer<Object> returnValue;
			WorkerProcedure workerProcedure;
#endif
			TPointer<Object> parameter;

			inline WorkerQueueNode() {
#ifdef XYO_PLATFORM_MULTI_THREAD
				started = false;
#endif
#ifdef XYO_PLATFORM_SINGLE_THREAD
				workerProcedure = nullptr;
#endif
			};

			inline void activeDestructor() {
#ifdef XYO_PLATFORM_MULTI_THREAD
				worker.endWork();
				started = false;
#endif
#ifdef XYO_PLATFORM_SINGLE_THREAD
				returnValue.deleteMemory();
				workerProcedure = nullptr;
#endif
				parameter.deleteMemory();
			};
	};

	class WorkerQueue : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(WorkerQueue);

		protected:
			int numberOfThreads;
			TDynamicArray<WorkerQueueNode> queue;
			bool allDone;

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
