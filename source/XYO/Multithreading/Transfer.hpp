// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_MULTITHREADING_TRANSFER_HPP
#define XYO_MULTITHREADING_TRANSFER_HPP

#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP
#	include <XYO/Multithreading/Dependency.hpp>
#endif

#ifndef XYO_MULTITHREADING_SEMAPHORE_HPP
#	include <XYO/Multithreading/Semaphore.hpp>
#endif

namespace XYO::Multithreading {

	typedef TPointer<Object> (*TransferProcedure)(Object *);

	template <typename T, TPointer<T> FunctionT(T &)>
	struct TGetTransferProcedure {
			static TPointer<Object> transferProcedure(Object *this_);
	};

	template <typename T, TPointer<T> FunctionT(T &)>
	TPointer<Object> TGetTransferProcedure<T, FunctionT>::transferProcedure(Object *this_) {
		if (this_ == nullptr) {
			return nullptr;
		};
		return TStaticCast<Object *>(FunctionT(*(static_cast<T *>(this_))));
	};

#ifdef XYO_PLATFORM_MULTI_THREAD

	//
	// Two linked Transfer objects, one used by each thread, pass objects
	// between the two threads (a.link(&b)):
	// - set() posts a value to the other side, wakes it, then waits until
	//   the other side took it with get()
	// - get() takes the posted value, copied by transferProcedure in the
	//   calling thread, and releases the other side
	// - hasValue() true if the other side posted a value
	// - waitValue() sleeps until the other side posts a value or calls
	//   notifyPeer(), check hasValue() after, it can return without a value
	// - notifyPeer() wakes the other side from waitValue()
	// - setNotify() an extra semaphore notified with the own waitValue()
	//   signal, for one thread waiting on several Transfer objects;
	//   kept by link(), must outlive the link
	//
	class Transfer : public Object {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(Transfer);

		protected:
			Transfer *thread1;
			Transfer *thread2;
			Object *value1;
			Object *value2;
			TAtomic<bool> hasValue1;
			TAtomic<bool> hasValue2;
			Semaphore sync1;
			Semaphore sync2;
			// notified by the other side: value posted or notifyPeer()
			Semaphore valueSignal;
			Semaphore *notify;

			void signalValue();

		public:
			XYO_MULTITHREADING_EXPORT Transfer();
			XYO_MULTITHREADING_EXPORT ~Transfer();
			XYO_MULTITHREADING_EXPORT void link(Transfer *this_);
			XYO_MULTITHREADING_EXPORT void set(Object *value_);
			XYO_MULTITHREADING_EXPORT TPointer<Object> get(TransferProcedure transferProc);
			XYO_MULTITHREADING_EXPORT bool hasValue();
			XYO_MULTITHREADING_EXPORT void waitValue();
			XYO_MULTITHREADING_EXPORT void notifyPeer();
			XYO_MULTITHREADING_EXPORT void setNotify(Semaphore *notify_);
	};

#endif

}

#endif
