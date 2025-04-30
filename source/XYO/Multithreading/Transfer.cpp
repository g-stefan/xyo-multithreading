// Multithreading
// Copyright (c) 2016-2025 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2025 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Transfer.hpp>

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	Transfer::Transfer() {
		link(nullptr);
	};

	Transfer::~Transfer(){};

	void Transfer::link(Transfer *this_) {
		if (this_) {
			thread1 = this_;
			thread2 = nullptr;
			this_->thread1 = nullptr;
			this_->thread2 = this;
			return;
		};
		thread1 = nullptr;
		thread2 = nullptr;
		value1 = nullptr;
		value2 = nullptr;
		hasValue1.set(false);
		hasValue2.set(false);
	};

	void Transfer::set(Object *value_) {
		if (thread1) {
			value1 = value_;
			hasValue1.set(true);
			sync1.wait();
		};
		if (thread2) {
			value2 = value_;
			hasValue2.set(true);
			sync2.wait();
		};
	};

	TPointer<Object> Transfer::get(TransferProcedure transferProcedure) {
		TPointer<Object> retV;
		if (thread1) {
			if (transferProcedure) {
				retV = (*transferProcedure)(thread1->value2);
			};
			thread1->hasValue2.set(false);
			thread1->sync2.notify();
		};
		if (thread2) {
			if (transferProcedure) {
				retV = (*transferProcedure)(thread2->value1);
			};
			thread2->hasValue1.set(false);
			thread2->sync1.notify();
		};
		return retV;
	};

	bool Transfer::hasValue() {
		if (thread1) {
			return thread1->hasValue2.get();
		};
		if (thread2) {
			return thread2->hasValue1.get();
		};
		return false;
	};

};

#endif
