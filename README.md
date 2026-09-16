*This project has been created as part of the 42 curriculum by feel-idr*

# Codexion

## Description

Codexion simulates coders sharing a circle of USB dongles in order to compile
quantum code. Each coder is a thread that repeatedly compiles (holding two
dongles at once), debugs, and refactors. A coder burns out if they do not start
a new compile within `time_to_burnout` milliseconds of the start of their
previous one.

The simulation ends in exactly one of two ways: a coder burns out, or every
coder has compiled at least `number_of_compiles_required` times.

The goal of the project is resource arbitration under deadlines: no deadlock, no
starvation, no data races, and burnout reported within 10 ms of the moment it
happens.

---

## Instructions

```bash
make
./codexion <nb_coders> <burnout_ms> <compile_ms> <debug_ms> <refactor_ms> <nb_compiles> <cooldown_ms> <fifo|edf>
```

| Rule | Effect |
| --- | --- |
| `make` | builds `codexion` with `-Wall -Wextra -Werror -pthread` |
| `make clean` | removes the object directory |
| `make fclean` | also removes the binary |
| `make re` | full rebuild |

All eight arguments are mandatory. Values must be integers with no sign, no
decimal point and no leading `+`; `time_to_burnout` must be greater than 0;
`number_of_coders` must be between 1 and 200; the scheduler must be exactly
`fifo` or `edf`. Anything else prints usage on `stderr` and exits with status 1.

### Usage examples

```bash
./codexion 4 800 200 100 100 3 50 fifo   # everyone finishes 3 compiles
./codexion 5 2000 200 200 200 7 0 edf    # same, earliest-deadline-first
./codexion 1 800 200 200 200 10 0 fifo   # one dongle only: coder 1 burns out
./codexion 5 500 200 200 200 10 0 fifo   # cycle longer than the deadline: burnout
```

Output format, one state change per line:

```
0 1 has taken a dongle
1 1 has taken a dongle
1 1 is compiling
201 1 is debugging
301 1 is refactoring
801 2 burned out
```

### Edge cases

- **One coder**: there is a single dongle, so the coder takes it, can never hold
  two, and burns out on schedule.
- **`number_of_compiles_required` = 0**: nothing has to be done, the program
  exits immediately with status 0 and prints nothing.
- **Infeasible parameters**: if `compile + debug + refactor` is already larger
  than `time_to_burnout`, a burnout is unavoidable by construction and the
  program reports it rather than hiding it.

---

## Scheduler

Every dongle owns its own request queue, implemented as a binary min-heap
(`heap.c`, `heap_utils.c`). A coder that wants a dongle pushes a request and is
only granted the dongle when its request is the root of that heap, so a newly
arriving coder can never barge in front of someone who is already waiting.

### FIFO

Requests are keyed by a simulation-wide arrival counter (`sim->seq`), so the
dongle is granted in the order the requests were registered.

### EDF

Requests are keyed by `last_compile_start + time_to_burnout`, so the coder
closest to burning out is served first. Equal deadlines are broken by the lower
coder ID, which makes the policy fully deterministic even when two timestamps
land in the same millisecond.

Switching policy touches exactly one function, `heap_less`. The queue machinery
itself is policy-blind.

---

## Technical choices

- **Queue capacity of two (`HEAP_CAPACITY`).** Dongle `d` is the `left` of coder
  index `d` and the `right` of coder index `d - 1`, and no other coder ever
  names it, so only two coders can ever queue for a given dongle. Each of those
  two can have at most one outstanding request, because its entry leaves the
  queue by `heap_pop` (granted) or `heap_remove` (cancelled) before it can push
  again. Two slots therefore always suffice, the queue never grows, and total
  queue memory stays O(n) instead of O(n squared).
- **A heap rather than a sorted list.** Push and pop are O(log n), and the
  comparison function is the only place the scheduling policy lives, so FIFO and
  EDF differ by one comparator rather than by two code paths. At this capacity
  the sift loops execute at most one iteration.
- **Absolute milliseconds.** All internal timestamps are milliseconds since the
  epoch from `gettimeofday`. Printed timestamps subtract the simulation start.
  Using absolute values means a cooldown deadline can be handed straight to
  `pthread_cond_timedwait`.
- **No allocation after startup.** Every request is a stack value copied into a
  queue allocated during initialisation, so no `malloc` runs while threads are
  active and no per-request memory can leak.
- **Cancellation.** `heap_remove` exists so a coder that gives up because the
  simulation ended does not leave a stale request blocking the queue root.

---

## Blocking cases handled

### Deadlock prevention and Coffman's conditions

A deadlock needs all four Coffman conditions at once. This implementation keeps
three of them and deliberately breaks the fourth:

| Condition | Status | Why |
| --- | --- | --- |
| Mutual exclusion | **kept** | a dongle is physically exclusive; that is the point of the exercise |
| Hold and wait | **kept** | a coder holds its first dongle while queueing for the second |
| No preemption | **kept** | a dongle is never taken back from a coder that holds it |
| Circular wait | **broken** | acquisition order is asymmetric |

Odd-numbered coders request their left dongle first, even-numbered coders
request their right dongle first (`take_two` in `dongle_pair.c`). In a circle
where at least one coder acquires in the opposite order, the wait-for graph
cannot close into a cycle, so no deadlock is possible. Even coders also wait 1 ms
before their first attempt, which staggers the opening burst and avoids a
thundering herd on the first round; this is an optimisation, not the deadlock
fix.

If a coder gets its first dongle but the simulation ends before it gets the
second, it releases the one it holds and leaves the loop, so no dongle is ever
abandoned while held.

### Starvation prevention

Arbitration is queue-based rather than "whoever wakes up first wins". Under FIFO
a coder is served in arrival order; under EDF a waiting coder's deadline stays
fixed while coders that have just compiled get later deadlines, so the waiting
coder reaches the root of the heap and is served.

### Cooldown handling

Releasing a dongle sets `free_at = now + dongle_cooldown`. `dongle_ready`
refuses to grant the dongle before that timestamp, so the cooldown is enforced at
the point of granting rather than by asking coders to be polite. A waiter blocks
on `pthread_cond_timedwait` and re-evaluates, so it wakes up as soon as the
cooldown expires without busy-waiting.

### Precise burnout detection

A dedicated monitor thread polls every 300 microseconds and compares
`now - last_compile_start` against `time_to_burnout` for each coder, skipping
coders that have already finished their required compiles. When it finds a
burnout it stops the simulation first and prints afterwards, so no other line can
slip in after the final message.

### Log serialization

Every message is printed while holding `print`, so two lines can never interleave
on one line of the terminal. State messages are additionally gated on the
`running` flag, so nothing is printed once the simulation is over. The burnout
message is the single deliberate exception: it is printed after the flag is
cleared, because it is the line that announces the end.

---

## Thread synchronization mechanisms

### Primitives

| Primitive | Protects | Used by |
| --- | --- | --- |
| `dongle.lock` (one per dongle) | `taken`, `free_at`, and that dongle's request queue | coders, monitor |
| `dongle.cond` (one per dongle) | waiting on a dongle becoming grantable | coders, monitor |
| `sim.state` | `running`, the arrival counter, and every coder's `last_compile` and `compiles` | coders, monitor |
| `sim.print` | `printf` output | coders, monitor |

Each dongle gets its own lock rather than sharing one global lock, so coders that
are not neighbours never contend with each other.

### Preventing race conditions

Every shared field has exactly one mutex that guards it, and no field is read
outside that mutex. The burnout check is the clearest example: a coder writes
`last_compile` and increments `compiles` under `sim.state`, and the monitor reads
both under the same mutex, so the monitor can never observe a half-updated coder
and kill someone who has just started compiling. Note that only one thread ever
writes a given coder's counters, so the mutex is not preventing a lost update; it
protects the writer against the monitor's concurrent read and forces the value to
be published to memory rather than cached in a register.

`sim->seq++` is the one genuinely multi-writer field: every coder increments it
when building a request. Without the mutex two requests could receive the same
sequence number and FIFO ordering between them would become undefined.

Likewise `taken`, `free_at` and the queue are only ever touched while holding
that dongle's own lock, which is what makes "two dongles can never be handed to
two coders at once" true rather than likely.

Configuration values (`burnout`, `compile_ms`, `nb_coders`, each coder's `id`,
`left`, `right`) are written before any thread is created and never modified
afterwards, so they are read without a lock. A data race requires a concurrent
writer; read-only sharing has none.

### Lock ordering

Only two nesting orders exist in the whole program, `dongle -> state` and
`print -> state`. The reverse orders never occur: nothing acquires a dongle lock
or the print lock while holding `state`. `stop_sim` in particular releases
`state` before it starts locking dongles to broadcast, and `do_compile` releases
`state` before calling `log_state`. Since no thread can hold a lock that another
thread wants in the opposite order, the mutexes themselves cannot deadlock,
independently of the dongle-ordering argument above.

### Coder/monitor communication

The two directions are separate on purpose. Coders report progress by writing
`last_compile` and `compiles` under `sim.state`; the monitor never touches coder
state, it only reads it.

In the other direction, the monitor signals the end through a custom broadcast
event: it clears `running` under `sim.state`, then walks every dongle and calls
`pthread_cond_broadcast` while holding that dongle's lock. Any coder blocked
waiting for a dongle wakes immediately, sees the flag, cancels its queued
request, and returns, so `main` can join all threads without a timeout and
without ever cancelling a thread from the outside. `precise_sleep` also checks
the flag while sleeping, so a coder in the middle of a long debug or refactor
also exits promptly instead of holding the program open.

---

## Memory management

Every heap allocation (the coder array, the dongle array, and one request queue
per dongle) is freed before the program exits, on all exit paths including
invalid arguments, burnout, and allocation failure during initialisation. The
`ready` counter records how many dongles were fully initialised so that cleanup
after a partial failure destroys exactly the mutexes and condition variables that
exist. All mutexes and condition variables are destroyed, and every created
thread is joined before cleanup begins.

---

## Resources

- [POSIX Threads Programming](https://hpc-tutorials.llnl.gov/posix/)
- [Dining Philosophers Problem](https://en.wikipedia.org/wiki/Dining_philosophers_problem)
- [Coffman's conditions for deadlock](https://en.wikipedia.org/wiki/Deadlock)
- [Earliest deadline first scheduling](https://en.wikipedia.org/wiki/Earliest_deadline_first_scheduling)
- `man pthread_cond_timedwait`, `man pthread_mutex_lock`, `man gettimeofday`

### Use of AI

> Replace this section with an accurate account of your own use before you turn
> the project in. The list below is a template, not a record of what you did.

AI was used for the following tasks:

- explaining concurrency concepts (Coffman's conditions, condition-variable
  semantics, why a lock hierarchy prevents mutex deadlock);
- reviewing the arbitration logic in `dongle.c` and the comparator in
  `heap_utils.c`;
- reviewing the queue capacity argument that reduced each dongle's request queue
  from `number_of_coders` slots to two.