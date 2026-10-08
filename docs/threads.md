# Threads

`XYO::Multithreading::Thread` runs a function on a new system thread that is
**registered with the managed memory**: around your procedure it calls
`RegistryThread::threadBegin()` / `threadEnd()`, so the thread has its own
pools and singletons for its lifetime, and they are freed when it ends.

Always start threads this way (directly, through the helpers below, or
through `Worker` / `WorkerQueue`). A thread started with `std::thread` or the
raw `XYO::Platform::Multithreading::Thread` crashes on its first managed
allocation.

---

## Thread

```cpp
typedef void (*ThreadProcedure)(void *);

class Thread : public Object {
	public:
		bool start(ThreadProcedure procedure, void *this_);
		void join();
		bool isRunning();
		void waitFinish();
		static void sleep(int milliSeconds);
		// helpers: onTimeout, setInterval, setIntervalActionFirst, onFinish
};
```

The procedure is a function pointer, not `std::function`: pass your state
through `this_` and cast it back inside. A lambda **without captures**
converts to it.

```cpp
struct Job {
	int input;
	int output;
};

void jobProcedure(void *this__) {
	Job *this_ = reinterpret_cast<Job *>(this__);
	this_->output = this_->input * 2;
};

Job job{21, 0};          // declared before the thread: outlives it
Thread thread;
if (thread.start(jobProcedure, &job)) {
	thread.join();       // job.output == 42
};
```

| Member | Behaviour |
|--------|-----------|
| `start(procedure, this_)` | Starts a new thread running `procedure(this_)`. Joins a previous run first (blocks if it is still running), so a `Thread` can be reused. Returns `false` if the OS could not create the thread. |
| `join()` | Waits for the procedure to end. Does nothing if not running. Any number of times. |
| `isRunning()` | `true` from `start()` until the thread has ended. A single atomic read. |
| `waitFinish()` | Sleeps (no polling) until the procedure has returned or thrown. Returns at once if not started. |
| `sleep(ms)` | Sleeps the calling thread. Negative values return at once. |
| destructor | **Joins.** Destroying a `Thread`, or releasing the last `TPointer<Thread>`, waits for the procedure to end. |

### Ownership

The thread that creates a `Thread` object is its **owner**:

- only the owner calls `start()`, `join()`, and destroys it;
- other threads may only read it: `isRunning()` and `waitFinish()`.

`Thread` is an `Object`: use it as a local, a plain member, or through
`TPointer<Thread>` (the helpers return one). It is not copyable or movable;
use an array or `TPointer` for several.

### Stopping a thread

There is no detach and no kill. Make the procedure end:

- loop on a `TAtomic<bool>` stop flag, or
- wait on a `Semaphore` / `ConditionVariable` that the owner signals, or
- for periodic work use `setInterval` with an `IntervalControl` (below).

Because the destructor joins, a procedure that never ends makes the owner
hang when the `Thread` goes out of scope.

### Exceptions

An exception escaping the procedure is caught and discarded by the
underlying platform thread (it cannot terminate the process). `waitFinish()`
and `onFinish` still see the thread as finished. Catch and report inside
your procedure if you care.

---

## Passing data to a thread

`this_` is a raw pointer. Three safe patterns:

**1. Plain data that outlives the thread** (as `Job` above). Shared fields
are protected with `TAtomic`, `CriticalSection` or a `Semaphore`. Declare
the data **before** the `Thread`: objects are destroyed in reverse order, so
the thread is joined before the data goes away.

**2. A managed object handed to the thread for good.** Allocate it with
`TMemorySystem` (the only allocator whose objects may be released on another
thread) and let the thread own it. This is exactly how the helpers below
work:

```cpp
class JobData : public Object {
	public:
		int value; // no links to objects of the creating thread
};

void jobDataProcedure(void *this__) {
	TPointer<JobData> data(reinterpret_cast<JobData *>(this__)); // owned by the new thread
	// ... use data->value, allocate freely: this thread has its own pools ...
};                                                               // released here, on the new thread

TPointer<Thread> startJob(int value) {
	TPointer<Thread> thread;
	thread.newMemory();
	JobData *data = TMemorySystem<JobData>::newMemory();
	data->value = value;
	if (thread->start(jobDataProcedure, data)) {
		return thread;          // do not touch data any more
	};
	TMemorySystem<JobData>::deleteMemory(data); // not started: release it here
	return nullptr;
};
```

**3. A copy made by the receiving thread**: `Worker`, `WorkerQueue`,
`Transfer`. Use this to get results back. See [Workers](workers.md).

Never pass a pooled object (`TPointer<T>::newMemory()`, containers, strings
of the creating thread) to another thread and use it on both sides: the
reference count is not atomic and the pool belongs to the creator.

---

## Helpers

All helpers start a new thread and return it as a `TPointer<Thread>`:

- **keep the returned pointer while the thread must run**. Releasing it joins:
  if you do not store it at all, the call blocks until the thread is done;
- everything passed by pointer or reference (`this_`, the control, the
  monitored thread) must **outlive the returned thread**. Declare it before
  the `TPointer<Thread>`;
- they return `nullptr` if the thread cannot be started.

### onTimeout

```cpp
static TPointer<Thread> onTimeout(int milliSeconds, ThreadProcedure procedure, void *this_);
```

Calls `procedure(this_)` once, after `milliSeconds`. It cannot be cancelled:
releasing the returned thread waits for the timeout and the call.

```cpp
Message message;     // outlives the timer
TPointer<Thread> timer = Thread::onTimeout(1000, showMessage, &message);
// ... other work ...
timer = nullptr;     // waits for showMessage, at most about 1 s from start
```

### setInterval and IntervalControl

```cpp
static TPointer<Thread> setInterval(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_);
static TPointer<Thread> setIntervalActionFirst(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_);

static TPointer<Thread> setInterval(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
static TPointer<Thread> setIntervalActionFirst(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
```

`setInterval` calls `procedure(this_)` every `milliSeconds`, the first call
after `milliSeconds`. `setIntervalActionFirst` makes the first call right
away. The interval is measured from the end of one call to the start of the
next (the time a call takes is not subtracted).

Prefer the **`IntervalControl`** overloads: `clear()` wakes the waiting
interval thread, so it stops right away. With the `TAtomic<bool>` overloads
the flag is checked every 10 ms, so stopping takes up to about 10 ms.

```cpp
struct Ticker {
	TAtomic<int> ticks;
};

void tick(void *this__) {
	Ticker *this_ = reinterpret_cast<Ticker *>(this__);
	this_->ticks.fetchAdd(1);
};

Ticker ticker;            // declared first, outlives the thread
IntervalControl control;  // declared first, outlives the thread
TPointer<Thread> thread = Thread::setInterval(control, 100, tick, &ticker);
// ... about 10 ticks per second ...
control.clear();          // stops right away, also from another thread
thread->join();           // after join no call runs any more
```

`IntervalControl`:

| Member | Behaviour |
|--------|-----------|
| `clear()` | Stop. From any thread, also from inside the callback. Wakes every interval thread using this control: one control can stop several intervals. |
| `isCleared()` | From any thread, lock free. |
| `reset()` | Use the control again. Only when no interval thread uses it. |
| `waitFor(ms)` | Used by the interval thread: waits `ms`, returns `false` at once when cleared. |

Stopping semantics, for both kinds of overload:

- `clear()` from inside the callback: that call is the last one.
- `clear()` from another thread: no new wait starts after it, but a call that
  is running (or has just finished its wait) completes. **`join()` or release
  the returned thread** before destroying anything the callback uses.
- An interval started with an already cleared control (or flag) makes no
  call and ends at once.
- `milliSeconds <= 0` calls the procedure back to back.

### onFinish

```cpp
static TPointer<Thread> onFinish(Thread &thread, ThreadProcedure procedure, void *this_);
```

Calls `procedure(this_)` once, after the procedure of `thread` ended
(returned or threw), or right away if `thread` is not started, so **start
`thread` first**. The monitor waits with `thread.waitFinish()`, no polling.
Several monitors on the same thread are all called.

`thread` is only read; it stays owned by the caller and **must be destroyed
after the monitor**:

```cpp
Work work;                     // used by both, declared first
{
	Thread thread;             // declared before the monitor, destroyed after it
	thread.start(workProcedure, &work);
	TPointer<Thread> monitor = Thread::onFinish(thread, finishProcedure, &work);
	// ...
};                             // monitor joined first, then thread
```

As class members, declare the monitored `Thread` before the
`TPointer<Thread>` monitor. In any other order, release the monitor first
(`monitor = nullptr;`). Before restarting a monitored `Thread`, release its
monitors.

### sleep

`Thread::sleep(milliSeconds)` sleeps the calling thread. In single thread
builds it exists as the free function `XYO::Multithreading::Thread::sleep`.
