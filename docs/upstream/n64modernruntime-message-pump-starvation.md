# `wait_for_external_message` can starve all message delivery (game-wide deadlock)

**File**: `ultramodern/src/mesgqueue.cpp`
**Found in**: WCW vs. nWo World Tour recomp (static recompilation, no decomp), but the
mechanism is game-agnostic.

## Symptom

The game deterministically freezes at a scene transition: every game thread parked in
`osRecvMesg`, the process alive and idle. Thread-stack sampling showed the VI thread
still enqueuing retrace messages — but they were never delivered.

## Root cause

`wait_for_external_message` processes **exactly one** message per wake, and on delivery
failure re-enqueues it **immediately**:

```cpp
void ultramodern::wait_for_external_message(RDRAM_ARG1) {
    QueuedMessage to_send;
    external_messages.wait_dequeue(to_send);
    if (!do_send(PASS_RDRAM to_send.mq, to_send.mesg, to_send.jam, false) && to_send.requeue_if_blocked) {
        external_messages.enqueue(to_send);
    }
}
```

`external_messages` is a moodycamel `BlockingConcurrentQueue`, which is **not FIFO across
producers**: a consumer preferentially dequeues from the producer sub-queue it last used.
When the pump thread re-enqueues a failed message, the pump thread itself becomes that
producer — so the next `wait_dequeue` hands the same message straight back.

If a `requeue_if_blocked` message targets a full 1-deep queue whose owner cannot run until
*some other* message is delivered (in our case: SI/PIF completion messages during a scene
transition, while the game thread waits on a VI retrace), the pump enters a closed
dequeue → fail → requeue loop on its own sub-queue and **never services any other
producer** — including the VI retrace events. Every thread ends up blocked in
`osRecvMesg`: total deadlock. Any game where an undeliverable requeued message coexists
with a message another thread needs first can hit this.

## Fix that worked for us

Drain everything available per wake; requeue failures only **after** the drain; back off
briefly if nothing was deliverable:

```cpp
void ultramodern::wait_for_external_message(RDRAM_ARG1) {
    QueuedMessage to_send;
    thread_local std::vector<QueuedMessage> requeued_messages;
    requeued_messages.clear();
    bool delivered_any = false;
    external_messages.wait_dequeue(to_send);
    do {
        if (do_send(PASS_RDRAM to_send.mq, to_send.mesg, to_send.jam, false)) {
            delivered_any = true;
        }
        else if (to_send.requeue_if_blocked) {
            requeued_messages.push_back(to_send);
        }
    } while (external_messages.try_dequeue(to_send));
    for (QueuedMessage& cur_mesg : requeued_messages) {
        external_messages.enqueue(cur_mesg);
    }
    if (!delivered_any && !requeued_messages.empty()) {
        ultramodern::sleep_milliseconds(1);
    }
}
```

`wait_for_external_message_timed` has the same single-message + immediate-requeue shape
and likely wants the same treatment.

With this change the game runs its full attract loop and is playable end to end;
previously it deadlocked at the same point every run (~12 s in, graphics task #331).
