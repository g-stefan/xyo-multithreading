---
name: xyo-multithreading
description: >-
  How to use the xyo-multithreading C++ library (namespace
  XYO::Multithreading), the threading layer of the XYO C++ stack on top of
  xyo-data-structures: Thread (managed thread registered with
  xyo-managed-memory, RAII join, waitFinish) and its helpers onTimeout,
  setInterval / setIntervalActionFirst with IntervalControl or a TAtomic<bool>
  flag, onFinish; moving objects between threads by copy with transfer
  procedures: Worker / TWorker (one background job at a time, result,
  requestToTerminate, hasFailed), WorkerQueue / TWorkerQueue (jobs on a thread
  pool, process, getReturnValue by index), Transfer (two-thread rendezvous
  channel), TGetTransferProcedure / TGetWorkerProcedure; Semaphore (binary
  signal), CriticalSectionLock, Synchronize<T>::process, and the re-exported
  CriticalSection, ConditionVariable, TAtomic, Processor. Use when writing or
  reviewing code that includes <XYO/Multithreading.hpp>, depends on
  "xyo-multithreading" in fabricare.json, starts threads or passes data
  between threads in XYO code, uses any of these names, or when working
  inside the xyo-multithreading repository or libraries built on it
  (xyo-system, quantum-script and its thread / job extensions).
---

# xyo-multithreading

Threading layer of the XYO C++ libraries, on top of `xyo-data-structures`
(see the `xyo-managed-memory`, `xyo-data-structures` and `xyo-platform`
skills; their rules all apply). Purpose: make threads work with the
**per-thread** managed memory model: every thread has its own pools and
object graphs, reference counts are not atomic. So this library (1) starts
threads registered with the memory manager and (2) moves objects between
threads **by copy**, made in the receiving thread while the sender waits.

Full documentation: `docs/` in the xyo-multithreading repository
(`X:\Storage\XYO\Gitea\CPP\xyo-multithreading\docs` on this machine):
README, getting-started, **threads**, **workers** (the copy model),
transfer, synchronization, reference. Read the matching page when you need
more than this summary. When in doubt read the header in
`source/XYO/Multithreading/`; tests in `test/` show real usage.

## Pick a tool

| Need | Use |
|------|-----|
| Run a function on a new thread | `Thread t; t.start(fn, this_);` - destructor joins |
| Once, later | `Thread::onTimeout(ms, fn, this_)` |
| Every N ms | `Thread::setInterval(control, ms, fn, this_)` + `IntervalControl::clear()`; `setIntervalActionFirst` calls at once |
| When a thread ends | `Thread::onFinish(thread, fn, this_)` |
| One background job at a time, with a result | `Worker` + `TWorker<...>` |
| Batch of jobs on a pool, all results | `WorkerQueue` + `TWorkerQueue<...>` |
| Own long lived thread exchanging objects | `Transfer` pair |
| Wake one thread from another | `Semaphore` (`notify` / `wait` / `waitFor` / `peek`) |
| Lock a scope / a lambda | `CriticalSectionLock lock(cs);` / `Synchronize<T>::process(cs, fn)` |
| Counter / flag | `TAtomic<T>` |

## Hard rules

1. **`main` first**: call `XYO::ManagedMemory::Registry::registryInit()` at
   the start of `main`, before any thread starts.
2. **Start threads only with `XYO::Multithreading::Thread`** (or helpers,
   `Worker`, `WorkerQueue`): it wraps the procedure in
   `RegistryThread::threadBegin()` / `threadEnd()`. Never `std::thread`, the
   raw `XYO::Platform::Multithreading::Thread`, or OS calls for code that
   allocates managed objects: it crashes on the first allocation. Never call
   `RegistryThread` yourself.
3. **One thread per object graph.** Never use an `Object`, `TPointer`, XYO
   container or XYO string from two threads, not even under a lock.
   Locks / `TAtomic` / `Semaphore` protect **plain data** only.
4. **Move managed objects by copy**: `Worker`, `WorkerQueue`, `Transfer`. To
   hand an object to a new thread for good, allocate it with
   `TMemorySystem<T>::newMemory()`, wrap it in `TPointer<T>` inside the
   thread procedure (the thread owns it), `deleteMemory` it if `start`
   failed, never touch it after `start`. It must not link to the creator's
   graph.
5. **Transfer procedure** `TPointer<T> copyT(T &source)`: runs in the
   receiving thread while the sender is blocked; returns a **new** object
   from the calling thread; copies plain fields, rebuilds managed members
   value by value. Never assign a `TPointer` / `TPointerX` / shared XYO
   string from `source`. May throw (job marked failed).
6. **Worker procedure** `TPointer<R> fn(P *parameter, TAtomic<bool>
   &requestToTerminate)`: `parameter` is its own copy (may be `nullptr`);
   long jobs poll `requestToTerminate.get()`; return `nullptr` for no result.
7. **Template parameter order**: `TWorker` / `TWorkerQueue<ReturnT,
   ParameterT, copyReturn, copyParameter, procedure>`. The untyped
   `WorkerQueue::add(procedure, transferReturnValue, transferParameter,
   parameter)` also has return **before** parameter.
8. **Thread ownership**: the creating thread alone calls `start`, `join`,
   destroys it; others may only `isRunning()` / `waitFinish()`. `start` on a
   running thread joins first. **The destructor joins**: the procedure must
   end (stop flag, `Semaphore`, `IntervalControl`). No detach, no kill.
   Exceptions escaping the procedure are swallowed.
9. **Helpers return `TPointer<Thread>`; releasing it joins.** Not storing it
   blocks the caller until done (a bare `Thread::onTimeout(1000, fn, p);`
   statement waits 1 s). `nullptr` if not started. Everything passed by pointer /
   reference (`this_`, control, monitored thread) must outlive the returned
   thread: **declare it before** the `TPointer<Thread>` (reverse
   destruction order), or release the thread first. `onTimeout` cannot be
   cancelled.
10. **Intervals**: prefer `IntervalControl` (`clear()` wakes the thread at
    once; the `TAtomic<bool>` overloads poll every 10 ms). After `clear()`
    from another thread a call already in progress (or just past its wait)
    still completes: `join()` / release the thread before destroying what
    the callback uses. `clear()` inside the callback makes that call the
    last. `reset()` only when no interval uses the control. Interval is
    from end of a call to start of the next; `ms <= 0` = back to back.
11. **`onFinish(thread, ...)`**: start `thread` first (not started = called
    at once); the monitored `Thread` must be destroyed **after** the
    monitor (declare it first; as members, declare the `Thread` before the
    `TPointer<Thread>`); release monitors before restarting it.
12. **Worker**: one owner thread. `start()` returns once the parameter is
    copied (caller may release it), joins a previous job first, starts the
    thread lazily; the thread lives between jobs until `endWork()` /
    destructor. Take the result after each job (`join()`, then
    `getReturnValue()`; `nullptr` if failed). `hasFailed()` = threw in a
    transfer or the procedure; the thread survives. `endWork()` requests
    termination and waits; procedures kept.
13. **WorkerQueue**: one owner thread. `process()` blocks until all jobs
    end, pool = `min(threads, jobs)` (default / `< 1` =
    `Processor::getCount()`), no polling; results by index:
    `TStaticCast<R *>(queue.getReturnValue(k))`, `hasFailed(k)`. Returns
    `false` if a job has no procedure (rest not started). After all done,
    `process()` does nothing until `reset()`: to run more, `reset()`,
    `add`, `process()`. `index(k)` **grows** the queue: check `length()`.
    Destructor waits for started jobs; unstarted ones are dropped.
14. **Transfer**: `a.link(&b)` before the threads start; each side used by
    one thread. `set(obj)` is a rendezvous: it blocks until the peer
    `get(TGetTransferProcedure<T, copyT>::transferProcedure)`. Both threads
    `set` at once = deadlock. Receiver loop: `if (hasValue()) get(...) else
    waitValue();` (may wake without a value). Stop it: set a flag, then
    `notifyPeer()`. Several channels to one thread: `setNotify(&semaphore)`.
15. **Semaphore**: binary, exactly one waiting thread; notify before wait is
    kept, several notifies = one; `wait` consumes, `peek` does not;
    `waitFor(ms)` false on timeout (full time). Good stop flag:
    `while (!stop.waitFor(ms)) { ... }`.
16. **CriticalSection is not recursive on Linux** (it is on Windows): never
    re-enter it from the same thread, also not via `CriticalSectionLock` /
    `Synchronize`. Name the lock: `CriticalSectionLock lock(cs);`.
17. **Single thread builds** (`XYO_PLATFORM_SINGLE_THREAD`): only
    `Thread::sleep` (free function), `WorkerQueue` / `TWorkerQueue`
    (sequential, no copy), `CriticalSectionLock`, `Synchronize`, plain
    `TAtomic`. No `Thread` class, `Worker`, `TWorker`, `Transfer`,
    `Semaphore`, `IntervalControl`, `ConditionVariable`: guard with
    `#ifdef XYO_PLATFORM_MULTI_THREAD`.

## Minimal patterns

```cpp
struct Number : public Object { int value; };
TPointer<Number> newNumber(int v) { TPointer<Number> r; r.newMemory(); r->value = v; return r; };
TPointer<Number> copyNumber(Number &n) { return newNumber(n.value); };
TPointer<Number> square(Number *p, TAtomic<bool> &) { return newNumber(p->value * p->value); };

typedef TWorkerQueue<Number, Number, copyNumber, copyNumber, square> SquareQueue;
typedef TWorker<Number, Number, copyNumber, copyNumber, square> SquareWorker;

int main(int, char *[]) {
	XYO::ManagedMemory::Registry::registryInit();

	WorkerQueue queue;
	for (int k = 0; k < 10; ++k) { SquareQueue::add(queue, newNumber(k)); };
	if (queue.process()) {
		TPointer<Number> r = TStaticCast<Number *>(queue.getReturnValue(3)); // 9
	};

	Worker worker;
	SquareWorker::set(worker);
	if (SquareWorker::start(worker, newNumber(7))) {
		worker.join();
		TPointer<Number> r = SquareWorker::getReturnValue(worker);       // 49
	};

	State state;                  // declared before the thread: outlives it
	IntervalControl control;
	TPointer<Thread> ticker = Thread::setInterval(control, 100, tick, &state);
	// ...
	control.clear();
	ticker = nullptr;             // joins
	return 0;
};
```

## Using it

- `#include <XYO/Multithreading.hpp>` (umbrella). C++17.
  `XYO::Multithreading` does `using namespace XYO::ManagedMemory;` and
  `XYO::DataStructures;` and re-exports `CriticalSection`,
  `ConditionVariable` (MT), `TAtomic`, `Processor`. `Version`, `Copyright`,
  `License` are ambiguous: qualify them in full
  (`XYO::Multithreading::Version::version()`).
- fabricare consumer: `"dependency": ["xyo-multithreading"]` (DLL) or
  `["xyo-multithreading.static"]` (exports `XYO_MULTITHREADING_LIBRARY`).
  Install `xyo-platform`, `xyo-managed-memory`, `xyo-data-structures`, then
  this library to the SDK before building dependents (see the `fabricare`
  skill).
- Without fabricare: compile the four amalgams (`Platform`,
  `ManagedMemory`, `DataStructures`, `Multithreading`) with
  `-DXYO_PLATFORM_LIBRARY -DXYO_MANAGEDMEMORY_LIBRARY
  -DXYO_DATASTRUCTURES_LIBRARY -DXYO_MULTITHREADING_LIBRARY` (MSVC: also
  `/DXYO_PLATFORM_COMPILE_STATIC`), the four `source/` dirs on the include
  path, `-pthread` on Linux.

## Code style (match the repository)

- Tabs (width 8), `.clang-format` in the repo, CRLF line endings; statements
  and blocks end with `};`.
- camelCase methods, `retV` for return values, `this__` for the `void *`
  thread argument and `this_` for its cast, trailing `_` on parameters that
  shadow members (`procedure_`).
- Headers: include guard `XYO_MULTITHREADING_<NAME>_HPP`, guarded includes
  (`#ifndef XYO_MULTITHREADING_DEPENDENCY_HPP #include ...`), MT-only code
  inside `#ifdef XYO_PLATFORM_MULTI_THREAD`. Add new headers to
  `source/XYO/Multithreading.hpp` and new `.cpp` files to
  `source/XYO/Multithreading.Amalgam.cpp`. Exported members use
  `XYO_MULTITHREADING_EXPORT`; classes holding OS / sync state are
  `XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE`.
- Waiting is done on `Semaphore` / `ConditionVariable`, never by polling
  with `sleep`; tests check this with timing (`test.05`, `test.06`).
- Data handed to a new thread internally: a `TMemorySystem` allocated
  `Object`, owned by the thread procedure (see `Thread.cpp`).
- SPDX header: MIT for `source/`, Unlicense for `test/`.
- Tests: `test/test.NN.cpp` plus a `"category": "test"` project in
  `fabricare.json`; a watchdog thread turns hangs into failures; run
  `fabricare make` then `fabricare test`.
