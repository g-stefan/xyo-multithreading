// Multithreading
// Copyright (c) 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
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

#ifdef XYO_MULTI_THREAD

	class Transfer : public Object {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Transfer);

		protected:
			Transfer *thread1;
			Transfer *thread2;
			Object *value1;
			Object *value2;
			TAtomic<bool> hasValue1;
			TAtomic<bool> hasValue2;
			Semaphore sync1;
			Semaphore sync2;

		public:
			XYO_MULTITHREADING_EXPORT Transfer();
			XYO_MULTITHREADING_EXPORT ~Transfer();
			XYO_MULTITHREADING_EXPORT void link(Transfer *this_);
			XYO_MULTITHREADING_EXPORT void set(Object *value_);
			XYO_MULTITHREADING_EXPORT TPointer<Object> get(TransferProcedure transferProc);
			XYO_MULTITHREADING_EXPORT bool hasValue();
	};

#endif

}

#endif
