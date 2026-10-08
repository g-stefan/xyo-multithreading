# Workers

`Worker` runs one job at a time on a background thread and gives its result
back. `WorkerQueue` runs a list of jobs on a pool of threads and keeps every
result. Both move the parameter and the result between threads **by copy**,
with transfer procedures you provide.

---

## The copy model: transfer procedures

Every thread has its own pools and object graphs (see
[Getting started — threads](getting-started.md#5-threads)), so a job cannot
simply use the caller's parameter object, and the caller cannot simply keep
the job's result object. Instead:

```
owner thread                         worker thread
------------                         -------------
parameter (owner's pool)
start(parameter)   --- posts --->    copy = transferParameter(*parameter)   (worker's pool)
   ... blocked until copied ...
                                     result = procedure(copy, requestToTerminate)
join()             <--- posts ---    result
copy = transferReturnValue(*result)     ... blocked until copied ...
(owner's pool)                       result released (worker's pool)
```

A **transfer procedure** has the form

```cpp
TPointer<T> copyT(T &source);
```

It is called **in the receiving thread**, while the sending thread is
blocked waiting for it, so reading `source` is safe. It must return a new
object allocated in the calling thread and **must not share anything managed
with `source`**:

- copy plain fields (`int`, `double`, `bool`, plain structs, `std::string`);
- rebuild managed members (`TPointer`, containers, XYO strings) as new
  objects, element by element, from their values. Never assign a
  `TPointer` / `TPointerX` from `source`, or a shared copy on write string:
  that would share a non-atomic reference count between two threads.

```cpp
struct Range : public Object {
		int from;
		int to;
};

TPointer<Range> copyRange(Range &source) {
	TPointer<Range> retV;
	retV.newMemory();
	retV->from = source.from;
	retV->to = source.to;
	return retV;
};
```

Transfer procedures may throw: the job is then marked failed (see below).

---

## Worker procedure

```cpp
TPointer<ReturnT> procedure(ParameterT *parameter, TAtomic<bool> &requestToTerminate);
```

Runs on the worker thread with its own copy of the parameter (`nullptr` if
the job was started with no parameter). Allocate freely; return the result,
or `nullptr` for none. Long jobs should check `requestToTerminate.get()` and
return early when it is `true`: it is set by `Worker::requestToTerminate()`,
`endWork()`, the destructor, and `WorkerQueue::reset()`.

---

## Worker

```cpp
struct Sum : public Object {
		long long value;
};

TPointer<Sum> copySum(Sum &source) {
	TPointer<Sum> retV;
	retV.newMemory();
	retV->value = source.value;
	return retV;
};

TPointer<Sum> sumRange(Range *range, TAtomic<bool> &requestToTerminate) {
	TPointer<Sum> retV;
	retV.newMemory();
	retV->value = 0;
	for (int k = range->from; k < range->to; ++k) {
		if (requestToTerminate.get()) {
			return nullptr;              // stopped, no result
		};
		retV->value += k;
	};
	return retV;
};

//              ReturnT ParameterT copy return copy parameter procedure
typedef TWorker<Sum, Range, copySum, copyRange, sumRange> SumWorker;

void example() {
	Worker worker;
	SumWorker::set(worker);              // procedures, once

	TPointer<Range> range;
	range.newMemory();
	range->from = 0;
	range->to = 1000;

	if (SumWorker::start(worker, range)) { // returns once the parameter is copied
		// ... the owner is free to do other work here ...
		worker.join();
		TPointer<Sum> sum = SumWorker::getReturnValue(worker);
		if (sum) {                       // nullptr if failed or no result
			printf("%lld\n", sum->value); // 499500
		};
	};
};                                       // destructor stops the worker thread
```

| Member | Behaviour |
|--------|-----------|
| `TWorker<...>::set(worker)` | Sets the procedure and both transfer procedures. |
| `TWorker<...>::start(worker, parameter)` / `start(Object *)` | Starts the worker thread if needed, waits for a previous job to end, then posts the parameter. Returns after the parameter has been copied, so the caller may release it. `false` if no procedure is set or the thread cannot be started. |
| `join()` | Waits for the job to end (no polling) and takes its result. |
| `isRunning()` | `true` while a job runs. Also takes a posted result. |
| `TWorker<...>::getReturnValue(worker)` / `getReturnValue()` | The copied result of the last job, `nullptr` if none or failed. |
| `hasFailed()` | `true` if the last job threw: in the parameter transfer, the procedure, or the result transfer. The worker thread survives and takes the next job. |
| `requestToTerminate()` | Sets the job's `requestToTerminate` flag. Does not wait. |
| `beginWork()` | Starts the worker thread now (`start` does it on demand). |
| `endWork()` | Requests termination, waits for the running job, ends the worker thread. Procedures are kept: `start()` can be used again. |
| `setNotify(Semaphore *)` | An extra semaphore notified when a result is posted and when a job ends, for one thread waiting on several workers. Must outlive the worker thread. |
| destructor / `activeDestructor()` | `endWork()`; `activeDestructor()` also forgets the procedures and result, for recycled pooled workers. |

Rules:

- A `Worker` is used by **one owner thread**, the one that calls `start`,
  `join`, `getReturnValue`.
- The worker thread **stays alive between jobs**, sleeping until the next
  `start()`. Starting it is paid once.
- The worker thread posts its result and waits until the owner takes it.
  Call `join()` (or `isRunning()`, `getReturnValue()`) after each job: the
  next job cannot start before the result is taken, `start()` does it for
  you.
- Without the `TWorker` helper, wrap typed functions with
  `TGetWorkerProcedure<ReturnT, ParameterT, procedure>::workerProcedure` and
  `TGetTransferProcedure<T, copyT>::transferProcedure`, pass them to
  `setProcedure`, `setTransferParameter`, `setTransferReturnValue`, and cast
  the result with `TStaticCast<ReturnT *>(worker.getReturnValue())`.

---

## WorkerQueue

```cpp
typedef TWorkerQueue<Sum, Range, copySum, copyRange, sumRange> SumQueue;

void example() {
	WorkerQueue queue;
	queue.setNumberOfThreads(4);         // default: number of processors

	for (int k = 0; k < 100; ++k) {
		TPointer<Range> range;
		range.newMemory();
		range->from = k * 1000;
		range->to = (k + 1) * 1000;
		SumQueue::add(queue, range);     // kept by the queue, copied when the job starts
	};

	if (!queue.process()) {              // blocks until every job ended
		return;
	};

	long long total = 0;
	for (size_t k = 0; k < queue.length(); ++k) {
		if (queue.hasFailed(k)) {
			continue;
		};
		TPointer<Sum> sum = TStaticCast<Sum *>(queue.getReturnValue(k));
		if (sum) {
			total += sum->value;
		};
	};
};
```

How `process()` works:

- It starts `min(numberOfThreads, length())` pool threads, gives each one a
  job, and as a thread ends a job it takes the result (copied into the
  calling thread, stored in the job) and gives the thread the next job. Jobs
  start in the order they were added; they end in any order; results are
  read **by index**.
- It sleeps between events (no polling) and returns when every job ended.
  The pool threads are then ended.
- It returns `false` if a job has no procedure: jobs from that one on are
  not started (`queue.index(k).started`), jobs already running are left to
  end and are waited for by `reset()` or the destructor.
- A job that throws is marked failed, has no result, and the other jobs
  continue.
- Once all jobs are done, `process()` returns `true` without doing anything
  until `reset()`. To run more jobs: `reset()`, `add(...)`, `process()`.

| Member | Behaviour |
|--------|-----------|
| `TWorkerQueue<...>::add(queue, parameter)` | Appends a job. |
| `add(procedure, transferReturnValue, transferParameter, parameter)` | Untyped form; note the order: return value transfer **before** parameter transfer. |
| `setNumberOfThreads(n)` / `getNumberOfThreads()` | Pool size; `n < 1` means `Processor::getCount()`, the default. |
| `process()` | Runs all jobs, see above. Call it from the thread that owns the queue. |
| `getReturnValue(index)` | Result of job `index` as `TPointer<Object>`, `nullptr` if none, failed, or out of range. Cast with `TStaticCast<ReturnT *>`. |
| `hasFailed(index)` | `true` if job `index` threw. |
| `setParameter(index, parameter)` | Replace the parameter of a job before `process()`. |
| `index(index)` | The job node: `parameter`, `returnValue`, `started`, `done`, `failed`. Grows the queue if `index >= length()`: check `length()` first. |
| `length()`, `isEmpty()` | Number of jobs. |
| `reset()` | Ends the pool (a job still running is asked to terminate, its result dropped) and removes all jobs. |
| destructor | Waits for started jobs to end normally. Jobs not started are not done. |

`WorkerQueue` is used by **one owner thread**. In single thread builds,
`process()` runs the jobs one after another in the calling thread, passing
the parameter and keeping the result without copying.

---

## Choosing

| Situation | Use |
|-----------|-----|
| One background job, then another, overlapping with the owner's work | `Worker` |
| A batch of independent jobs, wait for all | `WorkerQueue` |
| A long lived second thread exchanging many messages with the first | [`Transfer`](transfer.md) |
| Fire and forget, no result | `Thread` with a `TMemorySystem` parameter, or `Thread::onTimeout` ([Threads](threads.md)) |
