# API reference

All public symbols, grouped by header, in namespace `XYO::Multithreading`.
Include `<XYO/Multithreading.hpp>` to get all of them.

"MT only" marks symbols that exist only in multi thread builds
(`XYO_PLATFORM_MULTI_THREAD`).

## `Dependency.hpp`

Includes `<XYO/DataStructures.hpp>`. Inside the namespace:

```cpp
using namespace XYO::ManagedMemory;
using namespace XYO::DataStructures;
using Platform::Multithreading::CriticalSection;
using Platform::Multithreading::ConditionVariable;   // MT only
using Platform::Multithreading::TAtomic;
namespace Processor = Platform::Multithreading::Processor;
```

| Macro | Value |
|-------|-------|
| `XYO_MULTITHREADING_EXPORT` | dllexport when `XYO_MULTITHREADING_INTERNAL` is defined, dllimport otherwise, empty when `XYO_MULTITHREADING_LIBRARY` is defined |

## `Thread.hpp`

MT only, except `Thread::sleep`.

```cpp
typedef Platform::Multithreading::ThreadProcedure ThreadProcedure; // void (*)(void *)

class Thread : public Object {
	public:
		Thread();
		~Thread();                                          // joins
		bool start(ThreadProcedure procedure, void *this_); // false if not started
		void join();
		bool isRunning();
		void waitFinish();                                  // any thread
		static void sleep(int milliSeconds);

		static TPointer<Thread> onTimeout(int milliSeconds, ThreadProcedure procedure, void *this_);
		static TPointer<Thread> setInterval(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
		static TPointer<Thread> setInterval(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_);
		static TPointer<Thread> setIntervalActionFirst(TAtomic<bool> &clearInterval, int milliSeconds, ThreadProcedure procedure, void *this_);
		static TPointer<Thread> setIntervalActionFirst(IntervalControl &control, int milliSeconds, ThreadProcedure procedure, void *this_);
		static TPointer<Thread> onFinish(Thread &thread, ThreadProcedure procedure, void *this_);
};
```

Single thread builds instead declare:

```cpp
namespace XYO::Multithreading::Thread {
	void sleep(int milliSeconds);
};
```

See [Threads](threads.md).

## `IntervalControl.hpp`

MT only.

```cpp
class IntervalControl {
	public:
		IntervalControl();
		void clear();                    // any thread, wakes the interval threads
		bool isCleared() const;          // any thread
		void reset();                    // only when no interval uses it
		bool waitFor(int milliSeconds);  // false when cleared
};
```

## `Semaphore.hpp`

MT only.

```cpp
class Semaphore {
	public:
		Semaphore();
		void wait();                     // one waiting thread only
		bool waitFor(int milliSeconds);  // false on timeout
		void notify();
		bool peek() const;
		void reset();
};
```

See [Synchronization](synchronization.md#semaphore).

## `Transfer.hpp`

```cpp
typedef TPointer<Object> (*TransferProcedure)(Object *);

template <typename T, TPointer<T> FunctionT(T &)>
struct TGetTransferProcedure {
		static TPointer<Object> transferProcedure(Object *this_); // nullptr -> nullptr
};

class Transfer : public Object {                                   // MT only
	public:
		Transfer();
		~Transfer();
		void link(Transfer *this_);                                // nullptr unlinks
		void set(Object *value_);                                  // waits for get() on the other side
		TPointer<Object> get(TransferProcedure transferProc);
		bool hasValue();
		void waitValue();
		void notifyPeer();
		void setNotify(Semaphore *notify_);
};
```

See [Transfer](transfer.md).

## `Worker.hpp`

```cpp
typedef TPointer<Object> (*WorkerProcedure)(Object *parameter, TAtomic<bool> &requestToTerminate);

template <typename ReturnT, typename ParameterT, TPointer<ReturnT> FunctionT(ParameterT *, TAtomic<bool> &)>
struct TGetWorkerProcedure {
		static TPointer<Object> workerProcedure(Object *parameter, TAtomic<bool> &requestToTerminate);
};

class Worker : public Object {                                     // MT only
	public:
		Worker();
		~Worker();                                                 // endWork()
		void setProcedure(WorkerProcedure workerProcedure_);
		void setTransferParameter(TransferProcedure transferParameter_);
		void setTransferReturnValue(TransferProcedure transferReturnValue_);
		void setNotify(Semaphore *notify);

		bool beginWork();
		void endWork();

		bool start(Object *parameter);
		void join();
		bool isRunning();
		void requestToTerminate();
		TPointer<Object> getReturnValue();
		bool hasFailed();

		void activeDestructor();
};

template <typename ReturnT,
          typename ParameterT,
          TPointer<ReturnT> TransferReturnT(ReturnT &),
          TPointer<ParameterT> TransferParameterT(ParameterT &),
          TPointer<ReturnT> WorkerProcedureT(ParameterT *, TAtomic<bool> &)>
struct TWorker {                                                   // MT only
		static void set(Worker &worker);
		static bool start(Worker &worker, ParameterT *parameter);
		static TPointer<ReturnT> getReturnValue(Worker &worker);
};
```

See [Workers](workers.md#worker).

## `WorkerQueue.hpp`

```cpp
class WorkerQueueNode : public Object {
	public:
		WorkerProcedure workerProcedure;
		TransferProcedure transferParameter;    // MT only
		TransferProcedure transferReturnValue;  // MT only
		TPointer<Object> parameter;
		TPointer<Object> returnValue;
		bool started;                           // given to a thread
		bool done;                              // ended
		bool failed;                            // threw, no return value
		void activeDestructor();
};

class WorkerQueueThread : public Object {       // MT only, a pool thread
	public:
		Worker worker;
		size_t node;
		bool isBusy;
		void activeDestructor();
};

class WorkerQueue : public Object {
	public:
		WorkerQueue();
		~WorkerQueue();                         // waits for started jobs
		void add(WorkerProcedure workerProcedure_,
		         TransferProcedure transferReturnValue_,
		         TransferProcedure transferParameter_,
		         Object *parameter);
		void setNumberOfThreads(int numberOfThreads_); // < 1: Processor::getCount()
		int getNumberOfThreads();
		bool process();
		TPointer<Object> getReturnValue(size_t index);
		bool hasFailed(size_t index);
		void setParameter(size_t index, Object *parameter);
		void reset();
		WorkerQueueNode &index(size_t index);   // grows the queue
		size_t length() const;
		bool isEmpty() const;
};

template <typename ReturnT,
          typename ParameterT,
          TPointer<ReturnT> TransferReturnT(ReturnT &),
          TPointer<ParameterT> TransferParameterT(ParameterT &),
          TPointer<ReturnT> WorkerProcedureT(ParameterT *, TAtomic<bool> &)>
struct TWorkerQueue {
		static void add(WorkerQueue &workerQueue, ParameterT *parameter);
};
```

See [Workers](workers.md#workerqueue).

## `CriticalSectionLock.hpp`

```cpp
class CriticalSectionLock {
	public:
		CriticalSectionLock(CriticalSection &criticalSection_); // enter()
		~CriticalSectionLock();                                 // leave()
};
```

## `Synchronize.hpp`

```cpp
template <typename T>
struct Synchronize {
		template <typename F>
		static T process(CriticalSection &criticalSection, F &&fn); // return fn() under the lock
};
```

## `Version.hpp`, `Copyright.hpp`, `License.hpp`

```cpp
namespace XYO::Multithreading::Version {
	const char *version();
	const char *build();
	const char *versionWithBuild();
	const char *datetime();
};

namespace XYO::Multithreading::Copyright {
	const char *copyright();
	const char *publisher();
	const char *company();
	const char *contact();
};

namespace XYO::Multithreading::License {
	std::string license();
	std::string shortLicense();
};
```

Qualify these namespaces in full: the layers below have namespaces with the
same names.
