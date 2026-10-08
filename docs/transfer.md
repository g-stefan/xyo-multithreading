# Transfer

`Transfer` is the low level channel `Worker` is built on: two linked
`Transfer` objects, **one used by each thread**, pass objects between the two
threads by copy. Use it when you run your own long lived thread and exchange
many values with it. For "run this job and give me the result", use
[`Worker`](workers.md) instead.

```cpp
Transfer a;          // used only by thread A
Transfer b;          // used only by thread B
a.link(&b);          // before either thread uses them
```

| Member | Behaviour |
|--------|-----------|
| `link(&other)` | Links the two sides. `link(nullptr)` unlinks and clears the state. Only while no thread uses them. |
| `set(value)` | Posts `value` (an `Object *`, may be `nullptr`) to the other side, wakes it, then **waits until the other side took it** with `get()`. `value` must stay alive until `set` returns (it does if you hold a `TPointer` to it). |
| `get(transferProcedure)` | Takes the value the other side posted, copied **in the calling thread** by `transferProcedure`, and releases the other side from `set`. Returns `nullptr` if `transferProcedure` is `nullptr`. If the transfer procedure throws, the other side is still released and the exception propagates. |
| `hasValue()` | `true` if the other side posted a value not taken yet. |
| `waitValue()` | Sleeps until the other side posts a value or calls `notifyPeer()`. It can return without a value: check `hasValue()` after. A signal sent before the wait is not lost. |
| `notifyPeer()` | Wakes the other side from `waitValue()`, e.g. to make it see a stop flag. |
| `setNotify(Semaphore *)` | An extra semaphore notified together with this side's `waitValue()` signal, for one thread waiting on several `Transfer` objects. Kept by `link()`; must outlive the link. |

A transfer procedure has the type `TPointer<Object> (*)(Object *)`. Build it
from a typed copy function `TPointer<T> copyT(T &)` with
`TGetTransferProcedure<T, copyT>::transferProcedure` (it maps `nullptr` to
`nullptr`). The rules for writing one are in
[Workers — transfer procedures](workers.md#the-copy-model-transfer-procedures).

## Example: a consumer thread

```cpp
struct Message : public Object {
		int value;
};

TPointer<Message> newMessage(int value) {
	TPointer<Message> retV;
	retV.newMemory();
	retV->value = value;
	return retV;
};

TPointer<Message> copyMessage(Message &source) {
	return newMessage(source.value);
};

struct Channel {
		Transfer mainSide;       // used by the main thread
		Transfer threadSide;     // used by the consumer thread
		TAtomic<bool> stop;
		long long sum;           // read by main only after join()
};

void consumerProcedure(void *this__) {
	Channel *this_ = reinterpret_cast<Channel *>(this__);
	while (!this_->stop.get()) {
		if (this_->threadSide.hasValue()) {
			TPointer<Message> message = TStaticCast<Message *>(
			    this_->threadSide.get(TGetTransferProcedure<Message, copyMessage>::transferProcedure));
			this_->sum += message->value; // message belongs to this thread
			continue;
		};
		this_->threadSide.waitValue();    // sleeps, no polling
	};
};

void example() {
	Channel channel;                      // declared before the thread, outlives it
	channel.mainSide.link(&channel.threadSide);
	channel.stop.set(false);
	channel.sum = 0;

	Thread thread;
	if (!thread.start(consumerProcedure, &channel)) {
		return;
	};
	for (int k = 1; k <= 100; ++k) {
		channel.mainSide.set(newMessage(k)); // returns once the consumer copied it
	};
	channel.stop.set(true);
	channel.mainSide.notifyPeer();        // wake the consumer to see the stop flag
	thread.join();
	// channel.sum == 5050
};
```

## Rules

- **Each side is used by one thread.** The side you call `set` / `get` /
  `hasValue` / `waitValue` on is yours; the other side belongs to the other
  thread.
- `set` is a **rendezvous**: it does not queue. The sender is blocked until
  the receiver calls `get`. Both directions work, but the two threads must
  not `set` at the same time (each would wait for the other: deadlock).
  Alternate request / response, or use one direction per pair.
- Link before the threads start, unlink (`link(nullptr)`) only after both
  stopped using it. The two `Transfer` objects must outlive both threads'
  use of them.
- To stop a thread sleeping in `waitValue()`, set your stop flag, then
  `notifyPeer()`.
- To wait on several channels from one thread: give all of them the same
  `Semaphore` with `setNotify`, then loop "check every `hasValue()`, else
  `semaphore.wait()`". `WorkerQueue` does this with its pool.
