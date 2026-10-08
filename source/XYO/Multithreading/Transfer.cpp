// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Transfer.hpp>

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	Transfer::Transfer() {
		notify = nullptr;
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
		// unlinked, no other thread uses it
		sync1.reset();
		sync2.reset();
		valueSignal.reset();
	};

	// Wake the other side after the value is posted, not before:
	// woken before, it could find no value and wait again forever
	void Transfer::set(Object *value_) {
		if (thread1) {
			value1 = value_;
			hasValue1.set(true);
			thread1->signalValue();
			sync1.wait();
		};
		if (thread2) {
			value2 = value_;
			hasValue2.set(true);
			thread2->signalValue();
			sync2.wait();
		};
	};

	// The other side is blocked in set() until notified,
	// release it even if transferProcedure throws
	TPointer<Object> Transfer::get(TransferProcedure transferProcedure) {
		TPointer<Object> retV;
		if (thread1) {
			try {
				if (transferProcedure) {
					retV = (*transferProcedure)(thread1->value2);
				};
			} catch (...) {
				thread1->hasValue2.set(false);
				thread1->sync2.notify();
				throw;
			};
			thread1->hasValue2.set(false);
			thread1->sync2.notify();
		};
		if (thread2) {
			try {
				if (transferProcedure) {
					retV = (*transferProcedure)(thread2->value1);
				};
			} catch (...) {
				thread2->hasValue1.set(false);
				thread2->sync1.notify();
				throw;
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

	void Transfer::waitValue() {
		valueSignal.wait();
	};

	void Transfer::notifyPeer() {
		if (thread1) {
			thread1->signalValue();
		};
		if (thread2) {
			thread2->signalValue();
		};
	};

	void Transfer::setNotify(Semaphore *notify_) {
		notify = notify_;
	};

	// Called by the other side
	void Transfer::signalValue() {
		valueSignal.notify();
		if (notify) {
			notify->notify();
		};
	};

};

#endif
