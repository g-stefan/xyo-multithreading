// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/Multithreading.hpp>
#include <cstdlib>
#include <chrono>

using namespace XYO::Multithreading;

// Semaphore - binary signal, one thread waits, one thread notifies
//
// - notify() before wait() is not lost
// - several notify() before wait() count as one
// - wait() consumes the signal, peek() does not
// - reset() clears the signal
// - waitFor(): timeout, notified, notify before
// - ping-pong between two threads, wake up is not polling

typedef std::chrono::steady_clock Clock;

int elapsedMilliSeconds(Clock::time_point begin) {
	return (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - begin).count();
};

int errorCount = 0;

void check(bool condition, const char *message) {
	printf("%s %s\r\n", condition ? "[ OK ]" : "[FAIL]", message);
	if (!condition) {
		++errorCount;
	};
};

// A lost signal makes wait() block forever,
// the watchdog fails the test instead of hanging it

struct Watchdog {
		TAtomic<bool> done;
};

void watchdogProcedure(void *this__) {
	const int timeLimit = 30000;
	Watchdog *this_ = reinterpret_cast<Watchdog *>(this__);
	int k;

	for (k = 0; k < timeLimit; k += 10) {
		if (this_->done.get()) {
			return;
		};
		Thread::sleep(10);
	};

	printf("[FAIL] test did not end in %d ms, a wait() was not released\r\n", timeLimit);
	printf("* Error: test failed\n");
	fflush(stdout);
	std::_Exit(1);
};

void testSignal() {
	Semaphore semaphore;

	check(!semaphore.peek(), "new semaphore is not set");

	semaphore.notify();
	semaphore.wait();
	check(true, "notify() before wait() is not lost");
	check(!semaphore.peek(), "wait() consumes the signal");

	semaphore.notify();
	semaphore.notify();
	semaphore.notify();
	semaphore.wait();
	check(!semaphore.peek(), "several notify() before wait() count as one");

	semaphore.notify();
	check(semaphore.peek() && semaphore.peek(), "peek() does not consume the signal");
	semaphore.wait();
	check(!semaphore.peek(), "wait() after peek() consumes the signal");

	semaphore.notify();
	semaphore.reset();
	check(!semaphore.peek(), "reset() clears the signal");
};

// ---

struct PingPong {
		Semaphore ping;
		Semaphore pong;
		int value;
		int rounds;
		TAtomic<bool> isOk;
};

void pongProcedure(void *this__) {
	PingPong *this_ = reinterpret_cast<PingPong *>(this__);
	int k;
	for (k = 0; k < this_->rounds; ++k) {
		this_->ping.wait();
		// main thread increments first in each round
		if (this_->value != k * 2 + 1) {
			this_->isOk.set(false);
		};
		++this_->value;
		this_->pong.notify();
	};
};

struct NotifyLater {
		Semaphore semaphore;
		int milliSeconds;
};

void notifyLaterProcedure(void *this__) {
	NotifyLater *this_ = reinterpret_cast<NotifyLater *>(this__);
	Thread::sleep(this_->milliSeconds);
	this_->semaphore.notify();
};

void testWaitFor() {
	Semaphore semaphore;
	Clock::time_point begin;
	char message[256];
	bool isNotified;
	int time;

	semaphore.notify();
	check(semaphore.waitFor(0) && !semaphore.peek(), "waitFor(0) after notify() returns true, consumes the signal");

	begin = Clock::now();
	isNotified = semaphore.waitFor(100);
	time = elapsedMilliSeconds(begin);
	sprintf(message, "waitFor(100) without notify: returned %s after %d ms", isNotified ? "true" : "false", time);
	check((!isNotified) && (time >= 90) && (time < 2000), message);

	NotifyLater notifyLater;
	Thread thread;
	notifyLater.milliSeconds = 50;
	begin = Clock::now();
	if (!thread.start(notifyLaterProcedure, &notifyLater)) {
		check(false, "waitFor notified: start thread");
		return;
	};
	isNotified = notifyLater.semaphore.waitFor(10000);
	time = elapsedMilliSeconds(begin);
	thread.join();
	sprintf(message, "waitFor(10000) notified after 50 ms: returned %s after %d ms", isNotified ? "true" : "false", time);
	check(isNotified && (time < 5000), message);
};

void testPingPong() {
	PingPong pingPong;
	Thread thread;
	Clock::time_point begin;
	char message[256];
	int time;
	int k;

	pingPong.value = 0;
	pingPong.rounds = 1000;
	pingPong.isOk.set(true);

	if (!thread.start(pongProcedure, &pingPong)) {
		check(false, "ping-pong: start thread");
		return;
	};
	begin = Clock::now();
	for (k = 0; k < pingPong.rounds; ++k) {
		// value is changed by one thread at a time, handed over by the semaphores
		if (pingPong.value != k * 2) {
			pingPong.isOk.set(false);
		};
		++pingPong.value;
		pingPong.ping.notify();
		pingPong.pong.wait();
	};
	time = elapsedMilliSeconds(begin);
	thread.join();

	sprintf(message, "ping-pong between two threads, %d rounds, value %d", pingPong.rounds, pingPong.value);
	check(pingPong.isOk.get() && (pingPong.value == pingPong.rounds * 2), message);

	// polling with sleep(1) needs at least 1 ms per wait, 2 waits per round
	sprintf(message, "ping-pong %d rounds in %d ms, wake up without polling", pingPong.rounds, time);
	check(time < pingPong.rounds, message);
};

void test() {
	Watchdog watchdog;
	Thread watchdogThread;

	// keep output if process crashes
	setvbuf(stdout, nullptr, _IONBF, 0);

	watchdog.done.set(false);
	watchdogThread.start(watchdogProcedure, &watchdog);

	printf("- Semaphore\r\n");
	testSignal();
	testWaitFor();
	testPingPong();

	watchdog.done.set(true);
	watchdogThread.join();

	if (errorCount) {
		throw std::runtime_error("test failed");
	};

	printf("Done.\r\n");
};

int main(int cmdN, char *cmdS[]) {
	try {

		test();

		return 0;

	} catch (const std::exception &e) {
		printf("* Error: %s\n", e.what());
	} catch (...) {
		printf("* Error: Unknown\n");
	};

	return 1;
};
