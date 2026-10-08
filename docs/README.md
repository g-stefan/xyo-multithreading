# XYO Multithreading — Documentation

`xyo-multithreading` is the threading layer of the XYO C++ library stack. It
sits on top of `xyo-data-structures` and is **the** way to use threads with
the XYO managed memory model:

- **Threads that work with managed memory.** `Thread` registers every new
  thread with `xyo-managed-memory` for its lifetime, so the thread gets its
  own memory pools and can allocate `Object`s. A thread started any other way
  crashes on its first allocation.
- **Moving data between threads by copy.** `Transfer`, `Worker` and
  `WorkerQueue` hand an object to another thread by **copying it in the
  receiving thread** with a *transfer procedure* you write. The object itself
  never crosses the thread boundary, so reference counting stays non-atomic
  and lock free.
- **Work on other threads.** `Worker` runs one job at a time on a background
  thread and gives back its result; `WorkerQueue` runs a list of jobs on a
  thread pool and keeps every result.
- **Timers.** `Thread::onTimeout`, `setInterval`, `setIntervalActionFirst`
  and `onFinish` run a callback on a new thread later, periodically, or when
  another thread ends. `IntervalControl` stops an interval right away.
- **Synchronization helpers.** `CriticalSectionLock` (RAII lock),
  `Synchronize<T>::process` (run a lambda under a lock), `Semaphore` (a
  binary signal between two threads), plus the re-exported `CriticalSection`,
  `ConditionVariable`, `TAtomic` and `Processor` of `xyo-platform`.

```
quantum-script, applications ...
xyo-system, xyo-encoding, xyo-cryptography, ...
xyo-multithreading    <-- this library
xyo-data-structures   (containers, DynamicObject, TRing)
xyo-managed-memory    (Object, TPointer, TPointerX, per-thread pools)
xyo-platform          (macros, TAtomic, CriticalSection, ConditionVariable, raw Thread)
```

## Why it exists: multi-threading in isolation

`xyo-managed-memory` gives **every thread its own pools, singletons and
object graphs**. Reference counts and links are plain fields, not atomic, so
allocation and pointer copies are fast and need no lock. The price is a set
of rules:

1. a thread must be registered with the memory manager before it allocates;
2. an object graph is used by one thread only;
3. a pooled object is released by the thread that created it.

This library is built so that following the rules is the easy path:

| Rule | How the library keeps it |
|------|--------------------------|
| registered threads | `Thread::start` wraps the procedure in `RegistryThread::threadBegin()` / `threadEnd()` |
| one thread per graph | `Transfer` / `Worker` / `WorkerQueue` pass a **copy**, made by the receiving thread while the sender waits |
| release where created | the copy belongs to the receiver; the original stays with the sender; the `Thread` helpers hand data to a new thread with `TMemorySystem` (releasable anywhere) |

No thread ever touches an object of another thread's graph, except to read
it inside a transfer procedure while its owner is blocked.

## Concepts at a glance

| Need | Use | Notes |
|------|-----|-------|
| Run a function on a new thread | `Thread` | RAII: the destructor joins |
| Run something once, later | `Thread::onTimeout(ms, fn, this_)` | returns `TPointer<Thread>`, releasing it waits |
| Run something every N ms | `Thread::setInterval(control, ms, fn, this_)` | stop with `IntervalControl::clear()` |
| Run something when a thread ends | `Thread::onFinish(thread, fn, this_)` | no polling |
| One background job at a time, with a result | `Worker` + `TWorker<...>` | thread kept alive between jobs |
| Many jobs on a pool, keep all results | `WorkerQueue` + `TWorkerQueue<...>` | blocks in `process()` until all done |
| Pass objects between two threads you own | `Transfer` | rendezvous: `set()` waits for `get()` |
| Wake one thread from another | `Semaphore` | binary, notify before wait is not lost |
| Lock a scope | `CriticalSectionLock lock(cs);` | released on return or exception |
| Run a lambda under a lock | `Synchronize<T>::process(cs, fn)` | returns `fn()` |

## Contents

| Document | What it covers |
|----------|----------------|
| [Getting started](getting-started.md) | Build it, depend on it (DLL or static), first program, the threading rules, single thread builds, building without fabricare |
| [Threads](threads.md) | `Thread`, ownership, passing data to a thread, `onTimeout`, `setInterval`, `IntervalControl`, `onFinish` |
| [Workers](workers.md) | Transfer procedures, `Worker` / `TWorker`, `WorkerQueue` / `TWorkerQueue`, cancellation, failures |
| [Transfer](transfer.md) | The low level two-thread channel `Worker` is built on |
| [Synchronization](synchronization.md) | `CriticalSectionLock`, `Synchronize`, `Semaphore`, and the `xyo-platform` primitives |
| [API reference](reference.md) | Every public symbol on one page |

The memory model itself (`Object`, `TPointer`, `TPointerX`, allocators) is
documented in the `xyo-managed-memory` repository, `docs/`. The raw
primitives (`TAtomic`, `CriticalSection`, `ConditionVariable`) are documented
in the `xyo-platform` repository, `docs/multithreading.md`.

## Source map

```
source/XYO/Multithreading.hpp                umbrella header, include this
source/XYO/Multithreading.Amalgam.cpp        compiled part of the library in one translation unit
source/XYO/Multithreading/
    Dependency.hpp                           xyo-data-structures, export macro, re-exported names
    Thread[.cpp]                             managed thread, onTimeout / setInterval / onFinish
    IntervalControl[.cpp]                    stops setInterval right away
    Semaphore[.cpp]                          binary signal between two threads
    Transfer[.cpp]                           copy objects between two threads
    Worker[.cpp]                             background thread running one job at a time
    WorkerQueue[.cpp]                        jobs on a thread pool
    CriticalSectionLock.hpp                  RAII lock
    Synchronize.hpp                          call a function under a lock
    Copyright / License / Version            library metadata
test/test.01.cpp                             WorkerQueue + setInterval demo (Mandelbrot)
test/test.02.cpp                             exceptions in workers and transfer procedures
test/test.03.cpp                             per-thread memory: no object released on the wrong thread
test/test.04.cpp                             setInterval, IntervalControl
test/test.05.cpp                             Worker / WorkerQueue lifecycle, thread pool, no polling
test/test.06.cpp                             Semaphore
test/test.07.cpp                             Thread::onFinish
test/test.08.cpp                             CriticalSectionLock, Synchronize
```

## AI assistant skill

A Claude Code skill describing how to use this library lives in
[`.claude/skills/xyo-multithreading/`](../.claude/skills/xyo-multithreading/SKILL.md).
It is picked up automatically inside this repository; copy the folder to
`~/.claude/skills/` to have it available in the projects that depend on
`xyo-multithreading`.
