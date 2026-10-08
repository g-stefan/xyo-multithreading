# Synchronization

This library adds three tools on top of the `xyo-platform` primitives and
re-exports those primitives in `XYO::Multithreading`:

| Name | Header | What it is |
|------|--------|------------|
| `CriticalSectionLock` | `CriticalSectionLock.hpp` | RAII lock of a `CriticalSection` |
| `Synchronize<T>::process` | `Synchronize.hpp` | call a function with a `CriticalSection` entered |
| `Semaphore` | `Semaphore.hpp` | binary signal between two threads (multi thread builds) |
| `CriticalSection` | `xyo-platform` | mutex, `enter()` / `leave()` |
| `ConditionVariable` | `xyo-platform` | `wait`, `waitFor`, `notifyOne`, `notifyAll` (multi thread builds) |
| `TAtomic<T>` | `xyo-platform` | atomic value, acquire / release by default |
| `Processor::getCount()` | `xyo-platform` | number of logical processors |

The `xyo-platform` primitives are documented in its repository,
`docs/multithreading.md`. The pitfalls listed there apply here too, in
particular: **a `CriticalSection` is not recursive on Linux** (it is on
Windows). Never enter it twice from the same thread, also not through
`CriticalSectionLock` or `Synchronize`.

These tools protect **plain data** (numbers, flags, `std::` containers,
structs without managed pointers). Managed objects (`TPointer`, XYO
containers, XYO strings) are never shared between threads, even under a
lock: their pools belong to one thread. Move them with
[`Worker` / `Transfer`](workers.md).

---

## CriticalSectionLock

Enters the critical section on construction, leaves it on destruction, also
when the scope is left by `return` or an exception.

```cpp
class Counter {
		CriticalSection criticalSection;
		std::map<std::string, int> counts;   // protected by criticalSection

	public:
		void add(const std::string &key) {
			CriticalSectionLock lock(criticalSection);
			++counts[key];
		};

		int get(const std::string &key) {
			CriticalSectionLock lock(criticalSection);
			auto it = counts.find(key);
			return it == counts.end() ? 0 : it->second;
		};
};
```

Not copyable or movable. Give it a name: a temporary
`CriticalSectionLock{cs};` is destroyed at the end of the statement and
protects nothing.

## Synchronize

```cpp
template <typename T>
struct Synchronize {
		template <typename F>
		static T process(CriticalSection &criticalSection, F &&fn);
};
```

Calls `fn()` with the critical section entered and returns its value. `T`
can be any return type: `void`, a reference, a type without default
constructor. `fn` is any callable (lambda, function, `std::function`), called
directly. If `fn` throws, the lock is released and the exception
propagates.

```cpp
int value = Synchronize<int>::process(criticalSection, [&]() {
	return ++counter;
});

Synchronize<void>::process(criticalSection, [&]() {
	items.push_back(value);
});
```

---

## Semaphore

A **binary signal** between exactly two threads: one thread calls
`notify()`, the other calls `wait()`. The waiting thread sleeps on a
condition variable, no CPU, no polling.

| Member | Behaviour |
|--------|-----------|
| `notify()` | Sets the signal and wakes the waiting thread. Several `notify()` before a `wait()` count as one. |
| `wait()` | Blocks until the signal is set, then clears it (consumes it). A `notify()` done before `wait()` is **not lost**. |
| `waitFor(ms)` | As `wait()`, at most `ms` milliseconds (the full time, spurious wake ups are handled). Returns `false` on timeout. |
| `peek()` | The signal state, without clearing it. Lock free. |
| `reset()` | Clears the signal. |

Only **one** thread may `wait()` / `waitFor()` on a semaphore. `notify()`,
`peek()` and `reset()` are thread safe.

Unlike a bare `ConditionVariable`, the semaphore remembers the signal, so the
usual "check state in a loop under the lock" dance is not needed for a
simple hand-off:

```cpp
struct PingPong {
	Semaphore ping;   // main -> thread
	Semaphore pong;   // thread -> main
	int value;        // handed over by the semaphores, one thread at a time
};

void pongProcedure(void *this__) {
	PingPong *this_ = reinterpret_cast<PingPong *>(this__);
	for (int k = 0; k < 10; ++k) {
		this_->ping.wait();
		++this_->value;
		this_->pong.notify();
	};
};

void example() {
	PingPong pingPong;
	pingPong.value = 0;
	Thread thread;
	thread.start(pongProcedure, &pingPong);
	for (int k = 0; k < 10; ++k) {
		++pingPong.value;
		pingPong.ping.notify();
		pingPong.pong.wait();
	};
	thread.join();     // pingPong.value == 20
};
```

`notify()` / `wait()` also order memory: what one thread wrote before
`notify()` is visible to the other after its `wait()` returns.

As a **stop flag**: `notify()` once, `peek()` in the loop of the other
thread, or `waitFor(ms)` to sleep between iterations and wake at once on
stop:

```cpp
void loopProcedure(void *this__) {
	Semaphore *stop = reinterpret_cast<Semaphore *>(this__);
	while (!stop->waitFor(1000)) {   // every second, until stopped
		// ... periodic work ...
	};
};
```

(For periodic callbacks, `Thread::setInterval` with an `IntervalControl`
does this for you, see [Threads](threads.md#setinterval-and-intervalcontrol).)

---

## Which one

| Need | Use |
|------|-----|
| Protect a block of code | `CriticalSectionLock` |
| Protect one expression / lambda | `Synchronize<T>::process` |
| Counter or flag, no other data | `TAtomic<T>` (`fetchAdd`, `get`, `set`) |
| Wake one thread from another, hand over plain data | `Semaphore` |
| Several waiters, or a condition over shared state | `CriticalSection` + `ConditionVariable` (see `xyo-platform`) |
| Move managed objects | `Worker`, `WorkerQueue`, `Transfer` |
