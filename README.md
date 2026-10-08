# Multithreading

C++ library
- Threads that work with the XYO per-thread managed memory: `Thread`, with
`onTimeout`, `setInterval` (stopped right away by `IntervalControl`) and `onFinish`.
- Objects passed between threads by copy: `Worker` (one background job at a time),
`WorkerQueue` (jobs on a thread pool), `Transfer` (two-thread channel).
- Synchronization: `Semaphore`, `CriticalSectionLock`, `Synchronize`.

Built on `xyo-data-structures`; used by `xyo-system`, `quantum-script`
and the rest of the XYO C++ libraries.

## Documentation

- [Overview](docs/README.md) - purpose and design
- [Getting started](docs/getting-started.md) - build, depend on it, first program, threading rules
- [Threads](docs/threads.md) - `Thread`, passing data to a thread, `onTimeout`, `setInterval`, `IntervalControl`, `onFinish`
- [Workers](docs/workers.md) - transfer procedures, `Worker`, `WorkerQueue`
- [Transfer](docs/transfer.md) - the two-thread channel
- [Synchronization](docs/synchronization.md) - `CriticalSectionLock`, `Synchronize`, `Semaphore`
- [API reference](docs/reference.md)

A Claude Code skill for this library is in
[.claude/skills/xyo-multithreading](.claude/skills/xyo-multithreading/SKILL.md).

## License

Copyright (c) 2016-2026 Grigore Stefan
Licensed under the [MIT](LICENSE) license.
