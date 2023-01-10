// Multithreading
// Copyright (c) 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_WORKER_HPP
#define XYO_MULTITHREADING_WORKER_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifndef XYO_MULTITHREADING_THREAD_HPP
#	include <XYO/Multithreading/Thread.hpp>
#endif

#ifndef XYO_MULTITHREADING_TRANSFER_HPP
#	include <XYO/Multithreading/Transfer.hpp>
#endif

namespace XYO::Multithreading {

	typedef TPointer<Object> (*WorkerProcedure)(Object *parameter, TAtomic<bool> &requestToTerminate);

	template <typename ReturnT, typename ParameterT, TPointer<ReturnT> FunctionT(ParameterT *, TAtomic<bool> &)>
	struct TGetWorkerProcedure {
			static TPointer<Object> workerProcedure(Object *parameter, TAtomic<bool> &requestToTerminate);
	};

	template <typename ReturnT, typename ParameterT, TPointer<ReturnT> FunctionT(ParameterT *, TAtomic<bool> &)>
	TPointer<Object> TGetWorkerProcedure<ReturnT, ParameterT, FunctionT>::workerProcedure(Object *parameter, TAtomic<bool> &requestToTerminate) {
		return TStaticCast<Object *>(FunctionT(TStaticCast<ParameterT *>(parameter), requestToTerminate));
	};

#ifdef XYO_MULTI_THREAD

	class Worker : public Object {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Worker);

		protected:
			class Worker_ *worker;

		public:
			XYO_MULTITHREADING_EXPORT Worker();
			XYO_MULTITHREADING_EXPORT ~Worker();
			XYO_MULTITHREADING_EXPORT void setProcedure(WorkerProcedure workerProcedure_);
			XYO_MULTITHREADING_EXPORT void setTransferParameter(TransferProcedure transferParameter_);
			XYO_MULTITHREADING_EXPORT void setTransferReturnValue(TransferProcedure transferReturnValue_);

			XYO_MULTITHREADING_EXPORT bool beginWork();
			XYO_MULTITHREADING_EXPORT void endWork();

			XYO_MULTITHREADING_EXPORT bool start(Object *parameter);
			XYO_MULTITHREADING_EXPORT void join();
			XYO_MULTITHREADING_EXPORT bool isRunning();
			XYO_MULTITHREADING_EXPORT void requestToTerminate();
			XYO_MULTITHREADING_EXPORT TPointer<Object> getReturnValue();

			inline void activeDestructor() {
				endWork();
			};
	};

	template <typename ReturnT,
	          typename ParameterT,
	          TPointer<ReturnT> TransferReturnT(ReturnT &),
	          TPointer<ParameterT> TransferParameterT(ParameterT &),
	          TPointer<ReturnT> WorkerProcedureT(ParameterT *, TAtomic<bool> &)>
	struct TWorker {

			static inline void set(Worker &worker) {
				worker.setTransferReturnValue(TGetTransferProcedure<ReturnT, TransferReturnT>::transferProcedure);
				worker.setTransferParameter(TGetTransferProcedure<ParameterT, TransferParameterT>::transferProcedure);
				worker.setProcedure(TGetWorkerProcedure<ReturnT, ParameterT, WorkerProcedureT>::workerProcedure);
			};

			static inline bool start(Worker &worker, ParameterT *parameter) {
				return worker.start(TStaticCast<Object *>(parameter));
			};

			static inline TPointer<ReturnT> getReturnValue(Worker &worker) {
				return TStaticCast<ReturnT *>(worker.getReturnValue());
			};
	};

#endif

};

#endif
