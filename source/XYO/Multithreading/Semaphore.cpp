// Multithreading
// Copyright (c) 2022 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2022 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/Multithreading/Thread.hpp>
#include <XYO/Multithreading/Semaphore.hpp>

#ifdef XYO_MULTI_THREAD

namespace XYO::Multithreading {

	//
	// Simple Semaphore
	// Parent/Child mode
	// No Condition variable
	//

	Semaphore::Semaphore() {
		state.set(false);
	};

	void Semaphore::wait() {
		while (!state.get()) {
			Thread::sleep(1);
		};
		state.set(false);
	};

	void Semaphore::notify() {
		state.set(true);
	};

	bool Semaphore::peek() const {
		return state.get();
	};

	void Semaphore::reset() {
		state.set(false);
	};

};

#endif
