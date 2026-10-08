# Getting started

## 1. Build and install

The library is built with [fabricare](https://github.com/g-stefan/fabricare),
the build tool used by all XYO C++ projects. `xyo-platform`,
`xyo-managed-memory` and `xyo-data-structures` must be installed to the SDK
first. From the repository root:

```bash
fabricare make       # build into output/
fabricare test       # build and run test/test.*.cpp (run make first)
fabricare install    # copy output/{bin,include,lib} to ~/.fabricare/<platform>
fabricare clean      # remove output/ and temp/
```

Two libraries are produced:

| Project                       | Kind                                | Use it when                               |
|-------------------------------|-------------------------------------|-------------------------------------------|
| `xyo-multithreading`          | DLL / shared library (`dll-or-lib`) | default, shared between several libraries |
| `xyo-multithreading.static`   | static library, static CRT          | self-contained executables                |

## 2. Depend on it from another fabricare project

In the consumer's `fabricare.json`:

```json
{
	"name": "my-application",
	"make": "exe",
	"sourcePath": "XYO/MyApplication",
	"dependency": [
		"xyo-multithreading"
	]
}
```

For the static variant use `"xyo-multithreading.static"`. It exports
`XYO_MULTITHREADING_LIBRARY` to the consumer (`dependencyDefines`), which
turns `XYO_MULTITHREADING_EXPORT` into nothing. `xyo-data-structures`,
`xyo-managed-memory` and `xyo-platform` come in as transitive dependencies.

## 3. Include

```cpp
#include <XYO/Multithreading.hpp>
```

The umbrella header pulls in `<XYO/DataStructures.hpp>` (and through it
`<XYO/ManagedMemory.hpp>` and `<XYO/Platform.hpp>`) and every public header
of this library.

Namespace `XYO::Multithreading` contains:

```cpp
using namespace XYO::ManagedMemory;
using namespace XYO::DataStructures;
using Platform::Multithreading::CriticalSection;
using Platform::Multithreading::ConditionVariable;      // multi thread builds only
using Platform::Multithreading::TAtomic;
namespace Processor = Platform::Multithreading::Processor;
```

so `using namespace XYO::Multithreading;` is enough to write `Object`,
`TPointer`, `TDynamicArray`, `TAtomic`, `CriticalSection`, `Thread`, ...
unqualified. `Thread` means `XYO::Multithreading::Thread`; the raw
`XYO::Platform::Multithreading::Thread` is not brought in, and should not be
used directly (see [threads](#5-threads)).

The metadata namespaces exist in several layers (`Version`, `Copyright`,
`License`): write `XYO::Multithreading::Version::version()` in full.

Libraries built on top usually re-export the names in their own
`Dependency.hpp`:

```cpp
#include <XYO/Multithreading.hpp>

namespace XYO::MyLibrary {
	using namespace XYO::Multithreading;
};
```

## 4. First program

Square a list of numbers on a thread pool. Each job gets a *copy* of its
parameter, made in the pool thread, and its result is copied back in the
calling thread. See [Workers](workers.md) for the full story.

```cpp
#include <XYO/Multithreading.hpp>

using namespace XYO::Multithreading;

// Data passed between threads: an Object, no links to other objects
struct Number : public Object {
		int value;
};

TPointer<Number> newNumber(int value) {
	TPointer<Number> retV;
	retV.newMemory(); // allocated from the calling thread's pool
	retV->value = value;
	return retV;
};

// Transfer procedure: copy a Number into the calling thread
TPointer<Number> copyNumber(Number &number) {
	return newNumber(number.value);
};

// Worker procedure: runs on a pool thread, on its own copy of the parameter
TPointer<Number> square(Number *parameter, TAtomic<bool> &requestToTerminate) {
	return newNumber(parameter->value * parameter->value);
};

//                   ReturnT  ParameterT copy return  copy parameter  procedure
typedef TWorkerQueue<Number, Number, copyNumber, copyNumber, square> SquareQueue;

int main(int, char *[]) {
	XYO::ManagedMemory::Registry::registryInit(); // main thread first

	WorkerQueue queue;                    // number of threads = processors
	for (int k = 0; k < 10; ++k) {
		SquareQueue::add(queue, newNumber(k));
	};

	if (!queue.process()) {               // blocks until every job ended
		return 1;
	};

	for (size_t k = 0; k < queue.length(); ++k) {
		TPointer<Number> result = TStaticCast<Number *>(queue.getReturnValue(k));
		if (result) {                     // nullptr if the job threw
			printf("%zu^2 = %d\n", k, result->value);
		};
	};
	return 0;
};
```

As a fabricare test project:

```json
{
	"name": "test.09",
	"make": "exe",
	"category": "test",
	"dependency": [
		"xyo-multithreading"
	]
}
```

with the source in `test/test.09.cpp`.

## 5. Threads

The rules of `xyo-managed-memory` (its `docs/allocators.md#threads`), and how
to follow them with this library:

- **The main thread uses the memory manager first**, before any thread is
  started: call `XYO::ManagedMemory::Registry::registryInit()` at the start
  of `main` (allocating anything works too). If a secondary thread is first,
  the program crashes.
- **Start threads only with `XYO::Multithreading::Thread`** (or its helpers,
  `Worker`, `WorkerQueue`). It registers the new thread with the memory
  manager. Do not use `std::thread`, the raw `XYO::Platform` `Thread`, or OS
  calls for threads that allocate managed objects.
- **One thread per object graph.** Never use the same `Object`, or a
  `TPointer` to it, from two threads: reference counting is not atomic.
- **Move data between threads by copy**: `Worker`, `WorkerQueue`,
  `Transfer`. To hand an object to a new thread for good, allocate it with
  `TMemorySystem` (see [Threads — passing data](threads.md#passing-data-to-a-thread)).
- Plain data (ints, `TAtomic`, structs without managed pointers) may be
  shared like in any C++ program, protected by `CriticalSection` /
  `TAtomic`.

## 6. Single thread builds

With `XYO_PLATFORM_SINGLE_THREAD` (see `xyo-platform` configuration):

| Available | Not available |
|-----------|---------------|
| `Thread::sleep(ms)` (a free function in namespace `Thread`) | the `Thread` class and its helpers |
| `WorkerQueue`, `TWorkerQueue`: `process()` runs the jobs one after another in the calling thread, no copy | `Worker`, `TWorker`, `Transfer`, `Semaphore`, `IntervalControl`, `ConditionVariable` |
| `CriticalSectionLock`, `Synchronize` (the lock does nothing) | |
| `TAtomic` (a plain value) | |

Code that must build both ways uses `#ifdef XYO_PLATFORM_MULTI_THREAD`.

## 7. Building without fabricare

Compile the four amalgams with your sources:

1. Put `source/` of `xyo-platform`, `xyo-managed-memory`,
   `xyo-data-structures` and `xyo-multithreading` on the include path.
2. Provide the configuration headers of `xyo-platform` and
   `xyo-managed-memory` (see their documentation).
3. Compile `Platform.Amalgam.cpp`, `ManagedMemory.Amalgam.cpp`,
   `DataStructures.Amalgam.cpp` and `Multithreading.Amalgam.cpp` together
   with your sources, and define `XYO_PLATFORM_LIBRARY`,
   `XYO_MANAGEDMEMORY_LIBRARY`, `XYO_DATASTRUCTURES_LIBRARY` and
   `XYO_MULTITHREADING_LIBRARY` everywhere (plain static linking).
4. Link `pthread` on Linux.

Example on Linux, with the repositories side by side:

```bash
g++ -std=c++17 \
    -Ixyo-platform/source -Ixyo-managed-memory/source \
    -Ixyo-data-structures/source -Ixyo-multithreading/source \
    -DXYO_PLATFORM_LIBRARY -DXYO_MANAGEDMEMORY_LIBRARY \
    -DXYO_DATASTRUCTURES_LIBRARY -DXYO_MULTITHREADING_LIBRARY \
    xyo-platform/source/XYO/Platform.Amalgam.cpp \
    xyo-managed-memory/source/XYO/ManagedMemory.Amalgam.cpp \
    xyo-data-structures/source/XYO/DataStructures.Amalgam.cpp \
    xyo-multithreading/source/XYO/Multithreading.Amalgam.cpp \
    main.cpp -o main -pthread
```

On Windows with MSVC, also define `XYO_PLATFORM_COMPILE_STATIC`:

```bat
cl /EHsc /std:c++17 ^
   /Ixyo-platform\source /Ixyo-managed-memory\source ^
   /Ixyo-data-structures\source /Ixyo-multithreading\source ^
   /DXYO_PLATFORM_COMPILE_STATIC /DXYO_PLATFORM_LIBRARY /DXYO_MANAGEDMEMORY_LIBRARY ^
   /DXYO_DATASTRUCTURES_LIBRARY /DXYO_MULTITHREADING_LIBRARY ^
   xyo-platform\source\XYO\Platform.Amalgam.cpp ^
   xyo-managed-memory\source\XYO\ManagedMemory.Amalgam.cpp ^
   xyo-data-structures\source\XYO\DataStructures.Amalgam.cpp ^
   xyo-multithreading\source\XYO\Multithreading.Amalgam.cpp ^
   main.cpp
```
