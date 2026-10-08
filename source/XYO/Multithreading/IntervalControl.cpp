// Multithreading
// Copyright (c) 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/IntervalControl.hpp>
#include <XYO/Multithreading/CriticalSectionLock.hpp>

#include <chrono>

#ifdef XYO_PLATFORM_MULTI_THREAD

namespace XYO::Multithreading {

	IntervalControl::IntervalControl() {
		cleared.set(false);
	};

	// Set with criticalSection entered: a waiting thread tests cleared with
	// criticalSection entered, the clear can not happen between its test
	// and its wait
	void IntervalControl::clear() {
		CriticalSectionLock lock(criticalSection);
		cleared.set(true);
		conditionVariable.notifyAll();
	};

	bool IntervalControl::isCleared() const {
		return cleared.get();
	};

	void IntervalControl::reset() {
		CriticalSectionLock lock(criticalSection);
		cleared.set(false);
	};

	bool IntervalControl::waitFor(int milliSeconds) {
		CriticalSectionLock lock(criticalSection);
		if (cleared.get()) {
			return false;
		};

		// a wait can return early without notify, wait again the remaining time
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliSeconds);
		for (;;) {
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			if (now >= end) {
				return !cleared.get();
			};
			// round up, do not return before end
			int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - now + std::chrono::microseconds(999)).count();
			conditionVariable.waitFor(criticalSection, remaining);
			if (cleared.get()) {
				return false;
			};
		};
	};

};

#endif
