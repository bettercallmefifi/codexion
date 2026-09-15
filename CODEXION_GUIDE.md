# Codexion — Complete Teaching Guide

All five parts, in execution order.

---
---

# PART 1 — Foundations, the map, and program startup

## 0. How this guide is organised

| Part | Covers |
|---|---|
| **1** | Core concepts, the execution map, memory layout, `main.c`, `parse.c` |
| 2 | `init.c`, `heap.c`, `heap_utils.c` — the priority queue |
| 3 | `dongle.c`, `dongle_pair.c` — arbitration, the heart of the project |
| 4 | `coder.c`, `monitor.c`, `log.c`, `time_utils.c` |
| 5 | `cleanup.c`, full simulation, all lifecycles, races, deadlocks, weaknesses |

---

## 1. Concepts before any code

### 1.1 What compilation actually does

**Simple version.** Your `.c` files are text. The CPU cannot read text. Compilation turns text into machine instructions.

**Technical version.** Three stages:

1. **Preprocessing** — `#include "codexion.h"` is literally replaced by the entire contents of that file. `#define MAX_CODERS 200` makes the preprocessor replace every `MAX_CODERS` with `200` before the compiler ever sees it.
2. **Compiling and assembling** — each `.c` becomes a `.o` (object file) holding machine code, with holes where it calls functions defined elsewhere.
3. **Linking** — the linker joins all `.o` files and fills the holes. This is when `take_dongle` in `dongle.c` gets wired to the real code from `dongle.c`, and `printf` gets wired to the C library.

**In your project:**

```
src/main.c ──cc -c──> obj/main.o ─┐
src/parse.c ─cc -c──> obj/parse.o ┤
... 12 files ...                  ├──cc──> codexion
src/monitor.c cc -c─> obj/monitor.o ┘
```

This is why your Makefile has two rules. One compiles each `.c` to a `.o` separately, the other links all `.o` into `codexion`. Separate compilation means editing one file recompiles only that file.

The flag `-pthread` matters: it tells the compiler to enable thread support and links the pthread library, which provides `pthread_create`, `pthread_mutex_lock` and friends.

`-Wall -Wextra` turn on warnings. `-Werror` turns every warning into an error, so the build fails rather than producing a binary with a suspicious line in it.

### 1.2 Process vs thread

**Simple version.** A process is a running program. A thread is one worker inside that program. One process can have many threads, all working at the same time, all sharing the same memory.

**Technical version.** A process owns an address space: its code, its heap, its global data. A thread is a single flow of execution through that code. Each thread has its **own stack** and its **own CPU registers**, but shares **everything else** with the other threads in the process.

Registers are tiny storage slots inside the CPU itself. When your code says `i++`, the CPU loads `i` from memory into a register, adds one, and stores it back. Each thread has its own set of registers, so two threads can each be mid-calculation without corrupting each other's arithmetic.

**In your project.** Running `./codexion 4 ...` creates one process. That process then creates 6 threads total:

```
MAIN THREAD  (created by the OS when the program starts)
│
├── Coder thread 1   ── runs coder_routine()
├── Coder thread 2   ── runs coder_routine()
├── Coder thread 3   ── runs coder_routine()
├── Coder thread 4   ── runs coder_routine()
└── Monitor thread   ── runs monitor_routine()
```

All five created threads see the same `t_sim` struct. That is the entire point: they must coordinate over shared data.

The subject requires exactly this: "Each coder must be represented by a thread (using `pthread_create`)."

### 1.3 Stack vs heap

**Simple version.** The stack is a scratchpad that is automatically cleaned when a function returns. The heap is storage you request explicitly and must return explicitly.

**Technical version.**

The **stack** grows and shrinks as functions are called and return. When `parse_number` declares `long long value;`, that lives in `parse_number`'s stack frame. When `parse_number` returns, the frame is gone and `value` no longer exists. Stack memory is fast, automatic, and limited in size. **Each thread has its own stack.**

The **heap** is memory you get from `malloc` and keep until you call `free`. It survives across function calls. It is shared by all threads. If you `malloc` and never `free`, that is a memory leak.

**In your project:**

| Thing | Where it lives | Why |
|---|---|---|
| `t_sim sim` in `main()` | main thread's **stack** | Globals are forbidden by the subject, so the one shared struct lives in `main`'s frame and everyone gets a pointer to it |
| `sim.coders` array | **heap** (`malloc` in `init_coders`) | Size is unknown until arguments are parsed |
| `sim.dongles` array | **heap** (`malloc` in `init_dongles`) | Same reason |
| Each dongle's `queue.data` | **heap** (`malloc` in `heap_init`) | Same reason |
| `int i` inside any function | that thread's **stack** | Temporary, private to that call |

This is worth memorising for defense. A common question is "where does your data live and why is it not a global?" Answer: `sim` is a local variable of `main`, and every thread receives a pointer to it through the argument of `pthread_create`. The subject forbids globals; this is how you obey that rule while still sharing state.

### 1.4 Pointers

**Simple version.** A pointer is a variable that stores an address — where something else lives — rather than the thing itself.

**Technical version.** If `t_sim sim;` sits at address `0x7ffd1234`, then `t_sim *p = &sim;` means `p` holds the value `0x7ffd1234`. Writing `p->nb_coders` means "go to the address stored in `p`, then read the `nb_coders` field there". `p->x` is shorthand for `(*p).x`.

**Why this project needs them.** Five threads must all see and modify **the same** `t_sim`. If you passed a *copy* of the struct to each thread, each thread would modify its own private copy and nothing would coordinate. Passing the address means everyone works on one object.

**In your project:**

```
coder->sim
   |
   v
+--------------------+
|  the one t_sim     |   <-- sits in main()'s stack frame
|  running = 1       |
|  dongles ---------------> heap array of t_dongle
|  coders  ---------------> heap array of t_coder
+--------------------+
          ^
          |  every t_coder's `sim` field points back here
```

Notice this is circular: `sim.coders[0].sim == &sim`. Each coder can reach the shared world through its own `sim` pointer. That is why `coder_routine` only needs one argument.

### 1.5 Structs

**Simple version.** A struct bundles several variables into one named package.

**Technical version.** A struct is a contiguous block of memory with named fields at fixed offsets. `typedef struct s_coder { ... } t_coder;` creates the type and gives it the short name `t_coder` so you can write `t_coder` instead of `struct s_coder`.

Your header has one unusual line, line 40:

```c
typedef struct s_sim	t_sim;
```

This is a **forward declaration**. It says "a type called `t_sim` exists, details later". It is needed because `t_coder` (defined at line 42) contains a `t_sim *sim` field, while `t_sim` (defined at line 53) contains a `t_coder *coders` field. Each refers to the other. Without the forward declaration, whichever you wrote first would reference an unknown type. A pointer to an incomplete type is legal because the compiler only needs to know pointers are 8 bytes, not what they point at.

### 1.6 Race conditions — why any of this is hard

**Simple version.** Two threads touching the same variable at the same time can produce a wrong result, and the bug appears only sometimes.

**Technical version.** `coder->compiles++` is not one operation. It is three:

```
1. LOAD  compiles from memory into a register
2. ADD   1 to the register
3. STORE the register back to memory
```

The OS can pause a thread between any two of those steps.

There is a second, subtler problem: without synchronisation, the compiler is allowed to keep a variable in a register and never re-read it from memory, because as far as it can tell nothing else changes it. A reader thread can then loop forever on a stale value.

**The concrete race in your project.** Here is the one that actually matters. `coder->last_compile` is written by a coder thread and read by the monitor thread.

Imagine the mutex were removed from `do_compile` and `check_burnout`, with `time_to_burnout = 800`:

```
t=600   Coder 1 thread: begins writing last_compile = 600
                        (on some platforms a 64-bit write can be
                         split into two 32-bit stores)
t=600   Monitor thread: reads last_compile MID-WRITE
                        gets a torn value, e.g. 0
t=600   Monitor: now_ms() - 0 = 600  ... still under 800, fine
t=601   Monitor: now_ms() - 0 = 601  ... fine
        ... but the stored value stays torn ...
t=801   Monitor: 801 - 0 = 801 > 800  -> declares Coder 1 BURNED OUT
```

Coder 1 was compiling happily. The monitor killed the simulation because it read a value that was half-written. That is a data race, and the evaluation scale says a data race means 0 on the project.

**The fix, which your code uses.** `do_compile` writes under `sim->state`; `check_burnout` reads under the same `sim->state`. A mutex does two things: it makes the write-then-read sequence atomic, and it forces the compiler and CPU to actually publish and re-read the value from memory rather than caching it in a register.

Note carefully: in this project, `compiles` is incremented by **only one** coder thread. So the classic "two threads increment, one update is lost" story is *not* what the mutex is protecting against here. It protects a **writer against a concurrent reader** — the coder writes, the monitor reads. If an evaluator asks why you lock around `compiles++` when only one thread writes it, that is the answer, and it is a better answer than the textbook one.

---

## 2. The execution map

This is the whole program. Every arrow is a real function call from your code.

```
$ ./codexion 4 800 200 100 100 3 50 fifo
         │
         v
    ┌─────────────────────────── MAIN THREAD ───────────────────────────┐
    │ main()                                              [main.c:40]   │
    │   memset(&sim, 0, sizeof(t_sim))                                  │
    │   parse_args(&sim, ac, av)                          [parse.c:68]  │
    │        ├── parse_number()          check each number is valid     │
    │        ├── parse_times()           fill the six time fields       │
    │        ├── parse_sched()           "fifo" or "edf"                │
    │        └── usage_error()           on any failure                 │
    │   init_sim(&sim)                                    [init.c:45]   │
    │        ├── pthread_mutex_init(state), (print)                     │
    │        ├── init_dongles()  ── heap_init() per dongle              │
    │        └── init_coders()   ── set id/left/right/sim               │
    │   start_threads(&sim, &made)                        [main.c:4]    │
    │        ├── sim->start = now_ms()                                  │
    │        ├── pthread_create × nb_coders ────────┐                   │
    │        └── pthread_create × 1 (monitor) ──┐   │                   │
    │   join_threads()   blocks here ...        │   │                   │
    └───────────────────────────────────────────│───│───────────────────┘
                                                │   │
            ┌───────────────────────────────────┘   └──────────────┐
            v                                                      v
    ┌──── MONITOR THREAD ────┐              ┌──────── CODER THREAD × 4 ────────┐
    │ monitor_routine()      │              │ coder_routine()      [coder.c:31]│
    │  loop while running:   │              │  if nb_coders == 1 -> lone_coder │
    │   check_burnout(each)  │              │  if id even -> wait 1 ms         │
    │     -> stop_sim()      │              │  loop while running && !done:    │
    │     -> log_burnout()   │              │    take_two()     [dongle_pair.c]│
    │   all_done()           │              │      take_dongle(first)          │
    │     -> stop_sim()      │              │        build_request()           │
    │   usleep(300)          │              │        heap_push()               │
    └────────────────────────┘              │        wait_for_turn()           │
                                            │          dongle_ready()          │
            both threads end                │        heap_pop()                │
                    │                       │        log_state("has taken...") │
                    v                       │      take_dongle(second)         │
    ┌───────── MAIN THREAD resumes ───────┐ │    do_compile()                  │
    │   join_threads() returns            │ │      log_state("is compiling")   │
    │   destroy_sim()      [cleanup.c:26] │ │      precise_sleep(compile_ms)   │
    │     destroy_dongles() ── heap_free()│ │    drop_two() -> drop_dongle × 2 │
    │     free(coders)                    │ │    log_state("is debugging")     │
    │     pthread_mutex_destroy × 2       │ │    precise_sleep(debug_ms)       │
    │   return 0                          │ │    log_state("is refactoring")   │
    └─────────────────────────────────────┘ │    precise_sleep(refactor_ms)    │
                                            └──────────────────────────────────┘
```

---

## 3. Memory layout

```
MAIN THREAD STACK                         HEAP
┌──────────────────────────┐
│ t_sim sim                │
│  nb_coders   = 4         │
│  nb_compiles = 3         │
│  mode        = 0 (FIFO)  │
│  running     = 1         │
│  ready       = 4         │
│  burnout     = 800       │
│  compile_ms  = 200       │
│  debug_ms    = 100       │
│  refactor_ms = 100       │
│  cooldown    = 50        │
│  start       = 1757...   │
│  seq         = 37        │
│                          │        ┌──────────────────────────────┐
│  coders  ────────────────┼───────>│ t_coder[0] id=1 left=0 right=1│
│                          │        │            compiles=2         │
│                          │        │            last_compile=...   │
│                          │        │            thread=0x7f...     │
│                          │        │            sim ───────────────┼──┐
│                          │        │ t_coder[1] id=2 left=1 right=2│  │
│                          │        │ t_coder[2] id=3 left=2 right=3│  │
│                          │        │ t_coder[3] id=4 left=3 right=0│  │
│                          │        └──────────────────────────────┘  │
│                          │                                          │
│  dongles ────────────────┼───────>┌──────────────────────────────┐  │
│                          │        │ t_dongle[0] taken=1           │  │
│                          │        │             free_at=...       │  │
│                          │        │             queue ──> heap    │  │
│                          │        │             lock  (mutex)     │  │
│                          │        │             cond  (condvar)   │  │
│                          │        │ t_dongle[1] ...               │  │
│                          │        │ t_dongle[2] ...               │  │
│                          │        │ t_dongle[3] ...               │  │
│                          │        └──────────────────────────────┘  │
│  monitor (pthread_t)     │                                          │
│  state   (mutex)  <──────┼──────────────────────────────────────────┘
│  print   (mutex)         │        every coder's `sim` points back here
└──────────────────────────┘
```

Each dongle's `queue` is a `t_heap` **embedded inside** the dongle struct (not a pointer), but that heap's `data` field **is** a pointer to a separately malloc'd array:

```
t_dongle[2]
 ┌────────────────────┐
 │ taken    = 0       │
 │ free_at  = 1420    │
 │ queue:             │
 │   data ────────────┼────> [ {id:3,seq:12,dl:900}, {id:2,seq:15,dl:1100}, ... ]
 │   size     = 2     │        heap array, capacity = nb_coders
 │   capacity = 4     │
 │   mode     = 0     │
 │ lock  (mutex)      │
 │ cond  (condvar)    │
 └────────────────────┘
```

**Private vs shared.** This is the table an evaluator loves:

| Data | Private to one thread? | Shared? |
|---|---|---|
| `int i` in any function | Private (each thread's own stack) | No |
| `t_request req` in `build_request` | Private | No |
| `struct timespec ts` in `wait_for_turn` | Private | No |
| `coder->id`, `left`, `right` | Written once before threads start, then read-only | Read-shared, safe without a lock |
| `coder->compiles`, `coder->last_compile` | No | **Shared** — coder writes, monitor reads. Needs `state` |
| `sim->running` | No | **Shared** — monitor writes, everyone reads. Needs `state` |
| `sim->seq` | No | **Shared** — every coder increments. Needs `state` |
| `dongle->taken`, `free_at`, `queue` | No | **Shared** — needs that dongle's `lock` |
| `sim->burnout`, `compile_ms`, `start`, `nb_coders` | Written before threads start, never changed | Read-only, safe without a lock |

That last row is important and often misunderstood. `sim->compile_ms` is read by every thread with no mutex, and that is **correct**, because nothing ever writes it after `pthread_create`. Data races require at least one writer. Read-only sharing needs no protection.

---

## 4. `src/main.c`

### Purpose of the file

The entry point and lifecycle manager. It owns the single `t_sim` object, drives the four phases in order (parse → init → run → clean), and is the only place threads are created and joined.

---

### `main()`

**Purpose.** Own the shared state, run the four phases, return the correct exit status.

**Inputs.** `int ac` — argument count, including the program name. `char **av` — array of argument strings, `av[0]` being `"./codexion"`.

**Returns.** `0` on success, `1` on any failure.

**Variables.**

| Variable | Type | Meaning | Where the value comes from |
|---|---|---|---|
| `sim` | `t_sim` (the struct itself, not a pointer) | The entire shared world | Zeroed by `memset`, filled by `parse_args` and `init_sim` |
| `made` | `int` | How many coder threads were actually created | Set by `start_threads` through a pointer |
| `started` | `int` | 1 if all threads launched, 0 if any `pthread_create` failed | Return of `start_threads` |

**Line by line.**

```c
	memset(&sim, 0, sizeof(t_sim));
```
`sim` is a local variable, so its memory contains whatever junk was on the stack before. `memset` writes zero across all `sizeof(t_sim)` bytes. This matters because `destroy_sim` may run on an early failure path and calls `free(sim->coders)`. If `coders` held junk, `free` would crash. Zeroed, it is `NULL`, and `free(NULL)` is defined as doing nothing.

`&sim` is "address of sim" because `memset` needs a pointer. `sizeof(t_sim)` is computed by the compiler, so it stays correct if you add a field later.

```c
	if (!parse_args(&sim, ac, av))
		return (1);
```
`parse_args` returns 1 on success, 0 on failure, so `!parse_args(...)` is true on failure. Note we return **before** allocating anything, so there is nothing to free. Exit status 1 signals failure to the shell.

```c
	if (sim.nb_compiles == 0)
		return (0);
```
Edge case. If zero compiles are required, every coder has already satisfied the requirement, so there is nothing to simulate. Returning here avoids a subtle problem: `all_done` would immediately be true, and the program would spawn threads only to kill them a moment later, possibly printing a stray line first.

Note `sim.nb_compiles` with a dot, not an arrow, because `sim` is a struct here, not a pointer.

```c
	if (!init_sim(&sim))
	{
		destroy_sim(&sim);
		return (1);
	}
```
`init_sim` allocates. If it fails halfway — say the third dongle's heap could not be allocated — some memory is already allocated and some mutexes are already initialised. `destroy_sim` cleans up exactly what exists, using `sim->ready` to know how many dongles got fully initialised. Without this call, a failed init would leak.

```c
	made = 0;
	started = start_threads(&sim, &made);
	if (!started)
		stop_sim(&sim);
```
`made` is initialised before use. `start_threads` writes into it through a pointer. If thread creation failed partway, some coder threads are already running and looping. `stop_sim` sets `running = 0` and wakes everyone, so those threads exit instead of running forever and hanging `pthread_join`.

```c
	join_threads(&sim, made, started);
```
Joining means "wait until that thread has finished". This is the line where `main` sleeps for the entire simulation. We must join before `destroy_sim`, because destroying a mutex that a thread is still using is undefined behaviour, and freeing `coders` while a coder thread is reading it is a use-after-free.

```c
	destroy_sim(&sim);
	return (!started);
```
`!started` converts 1 (success) to 0 (shell success) and 0 (failure) to 1.

**Edge cases.**

| Case | What happens |
|---|---|
| `./codexion` with no args | `ac == 1`, `parse_args` prints usage, returns 1. No allocation, no leak |
| `./codexion 5 800 200 200 100 0 0 fifo` | `nb_compiles == 0`, returns 0 immediately, prints nothing |
| `malloc` fails in `init_coders` | `init_sim` returns 0, `destroy_sim` frees the dongles already allocated, returns 1 |
| `pthread_create` fails on coder 3 of 5 | `made == 2`, `stop_sim` wakes coders 1-2, `join_threads` joins exactly those 2, cleanup runs, returns 1 |

**What would go wrong without this function.** Without the join, `main` would return while coder threads still ran; the process would tear down memory under live threads, producing crashes that appear randomly. Without `destroy_sim`, every run leaks — an automatic 0 on the scale.

---

### `start_threads()`

**Purpose.** Stamp the start time, give every coder a starting deadline, then launch all threads.

**Inputs.** `t_sim *sim`; `int *made` — an **output parameter**, a pointer the function writes through so the caller can see how many threads were created.

**Returns.** 1 if every thread launched, 0 if any `pthread_create` failed.

**Line by line.**

```c
	sim->start = now_ms();
```
The reference instant. Every printed timestamp is `now_ms() - sim->start`, which is why the first line of output is `0` or `1` rather than a huge epoch number.

```c
	*made = 0;
	while (*made < sim->nb_coders)
	{
		sim->coders[*made].last_compile = sim->start;
		(*made)++;
	}
	*made = 0;
```
`*made` means "the int that `made` points at". Here it is reused as a plain loop counter, then reset to 0 before its real job. It is a little terse; it exists this way to stay inside the Norm's 25-line and 5-variable limits without adding another declaration.

The important line is `last_compile = sim->start`. The subject says a coder burns out if it has not started compiling within `time_to_burnout` of "the beginning of their last compile **or the beginning of the simulation**". Seeding `last_compile` with the start time is exactly that rule. If you left it at 0 (epoch), `now_ms() - 0` would be about 1.7 trillion, instantly greater than any burnout value, and every coder would be declared burned out on the monitor's first pass.

Note the order matters: this loop runs **before** any thread is created. If a coder thread started while its `last_compile` was still 0, the monitor could kill it in that window.

```c
		if (pthread_create(&sim->coders[*made].thread, NULL,
				coder_routine, &sim->coders[*made]) != 0)
			return (0);
```
Four arguments:
1. `&sim->coders[*made].thread` — where to store the new thread's handle, so we can join it later.
2. `NULL` — default thread attributes (default stack size, joinable).
3. `coder_routine` — a **function pointer**. Writing the function's name without parentheses gives its address. This is how you tell a thread what code to run. Its type must be `void *(*)(void *)`: takes a `void *`, returns a `void *`.
4. `&sim->coders[*made]` — the argument handed to `coder_routine`. Each thread gets the address of **its own** `t_coder`. This is how a thread knows "I am coder 3".

`pthread_create` returns 0 on success, so `!= 0` means failure.

**Why pass `&sim->coders[i]` and not a copy?** Because the monitor must see this coder's `compiles` and `last_compile`. A copy would be private and the monitor would watch a struct that never changes.

**A classic bug this code avoids.** A common mistake is:

```c
int i = 0;
while (i < n) { pthread_create(&t[i], NULL, routine, &i); i++; }   // WRONG
```

Every thread receives the address of the **same** `i`, which keeps changing. Threads would read unpredictable ids. Your code passes a distinct address per thread, so there is no such race.

```c
	if (pthread_create(&sim->monitor, NULL, monitor_routine, sim) != 0)
		return (0);
```
The monitor gets `sim` itself, because it supervises all coders rather than being one.

**Edge case: one coder.** `nb_coders == 1` means one coder thread plus the monitor. `coder_routine` detects this and calls `lone_coder`. Covered in Part 4.

---

### `join_threads()`

**Purpose.** Wait for every created thread to finish before cleanup.

**Inputs.** `t_sim *sim`, `int made` (how many coder threads exist), `int monitor_up` (whether the monitor exists).

**Returns.** Nothing.

```c
	if (monitor_up)
		pthread_join(sim->monitor, NULL);
```
Guarded, because if `pthread_create` for the monitor failed, `sim->monitor` holds garbage and joining it is undefined behaviour.

The monitor is joined **first** on purpose. The monitor is the thread that ends the simulation. Once it returns, `running` is already 0, so the coder threads are on their way out and the following joins return quickly.

```c
	i = 0;
	while (i < made)
	{
		pthread_join(sim->coders[i].thread, NULL);
		i++;
	}
```
`made`, not `nb_coders`. If only 2 of 5 threads were created, joining `coders[2].thread` would use an uninitialised handle.

The second argument `NULL` means "I do not want the thread's return value". `coder_routine` returns `NULL` anyway.

**Test yourself.** *What happens if you call `destroy_sim` before `join_threads`?*

Answer: `destroy_sim` calls `free(sim->coders)` and `pthread_mutex_destroy(&sim->state)`. A coder thread still inside `do_compile` would then lock a destroyed mutex and write to freed memory. You would get a crash or silent corruption, appearing on maybe one run in twenty. The scale says any segfault during defense is 0, and this class of bug is exactly the kind that survives your testing and fails during evaluation.

---

## 5. `src/parse.c`

### Purpose of the file

Turn eight text strings into validated numbers inside `t_sim`, and reject anything invalid before a single thread exists. The subject demands this: "Reject invalid inputs such as negative numbers, non-integers, or a scheduler other than fifo or edf."

**Why `atoi` is not used even though the subject allows it.** `atoi("abc")` returns 0 with no way to tell it apart from `atoi("0")`. `atoi("-5")` returns -5, and `atoi("99999999999")` overflows silently with undefined behaviour. None of those can be detected after the fact, so the code parses by hand.

---

### `parse_number()`

**Purpose.** Convert one string to a number, accepting **only** plain non-negative integers.

**Inputs.** `const char *str` — the text. `long long *out` — where to write the result on success.

**Returns.** 1 on success, 0 on rejection.

**Variables.**

| Variable | Type | Meaning |
|---|---|---|
| `value` | `long long` | The number built so far, digit by digit |
| `i` | `int` | Index of the character being read |

Why `long long` and not `int`? Because we must **detect** overflow. If `value` were an `int`, exceeding `INT_MAX` would overflow, which is undefined behaviour. A `long long` holds up to about 9.2 quintillion, so we can safely grow past 2147483647 and then check.

**Line by line.**

```c
	value = 0;
	i = 0;
	if (!str[0])
		return (0);
```
`str[0]` is the first character. For the empty string `""` the first character is `'\0'`, which is 0, so `!str[0]` is true. Without this check, an empty argument would skip the loop and store 0 — so `./codexion 5 800 200 200 100 5 "" fifo` would silently become cooldown 0.

```c
	while (str[i])
```
The loop runs until the terminating `'\0'`. In C, a string is a sequence of characters ending in a zero byte; `while (str[i])` is the idiomatic "until end of string".

```c
		if (str[i] < '0' || str[i] > '9')
			return (0);
```
**What is checked:** is this character a digit? Characters are numbers underneath; `'0'` is 48 and `'9'` is 57. Anything outside that range is not a digit.

**What it rejects, with real values:**

| Input | Rejected at | Why it matters |
|---|---|---|
| `"-5"` | `'-'` is 45, below `'0'` | Negative values are forbidden |
| `"8.5"` | `'.'` is 46 | Non-integers are forbidden |
| `"+800"` | `'+'` is 43 | Would otherwise be an odd accepted form |
| `"8o0"` | `'o'` is 111 | Typo caught rather than becoming 8 |
| `" fifo"` | `' '` is 32 | Stray whitespace caught |
| `"12abc"` | `'a'` | `atoi` would have returned 12 and hidden the garbage |

**If it were false** (the character is a digit), we continue to the conversion below.

```c
		value = value * 10 + (str[i] - '0');
```
The standard decimal build-up. `str[i] - '0'` converts the character to its numeric value: `'7'` is 55, `'0'` is 48, so `55 - 48 = 7`.

Trace with `"800"`:

| Step | `str[i]` | `value` before | Computation | `value` after |
|---|---|---|---|---|
| 1 | `'8'` | 0 | 0×10 + 8 | 8 |
| 2 | `'0'` | 8 | 8×10 + 0 | 80 |
| 3 | `'0'` | 80 | 80×10 + 0 | 800 |

```c
		if (value > 2147483647)
			return (0);
```
2147483647 is `INT_MAX`. The check is **inside** the loop, after each digit, not after the whole loop. That is deliberate: checking inside bounds `value` to at most about 21 billion before rejection, which is nowhere near overflowing a `long long`. If you only checked at the end, a 30-digit input would overflow `long long` itself and the check would be meaningless.

Why `INT_MAX` at all? Because `nb_coders` and `nb_compiles` are stored as `int` and `parse_times` casts to `int`. Accepting a larger value would truncate it into something unrecognisable.

```c
	*out = value;
	return (1);
```
The result is written **only on success**. On failure `*out` is untouched, so a caller can never accidentally use a half-parsed value.

---

### `parse_times()`

**Purpose.** Parse arguments 2 through 7 and store them in the right fields.

**Variables.** `long long val[6]` — an array holding the six parsed numbers before they are distributed. `int i` — loop index.

**Why an array instead of six separate calls?** Six separate calls plus six error checks would exceed the Norm's 25-line limit. The loop parses all six uniformly, then assigns.

```c
	while (i < 6)
	{
		if (!parse_number(av[i + 2], &val[i]))
			return (0);
		i++;
	}
```
The `+ 2` offset maps loop index to argument position:

| `i` | `av[i+2]` | Command-line position | Goes to |
|---|---|---|---|
| 0 | `av[2]` | time_to_burnout | `sim->burnout` |
| 1 | `av[3]` | time_to_compile | `sim->compile_ms` |
| 2 | `av[4]` | time_to_debug | `sim->debug_ms` |
| 3 | `av[5]` | time_to_refactor | `sim->refactor_ms` |
| 4 | `av[6]` | number_of_compiles_required | `sim->nb_compiles` |
| 5 | `av[7]` | dongle_cooldown | `sim->cooldown` |

`av[0]` is the program name and `av[1]` is `nb_coders`, handled separately, hence the offset of 2.

The function returns 0 **immediately** on the first bad argument, so later arguments are not parsed and `sim` is left partly filled — harmless, since the caller aborts.

```c
	sim->nb_compiles = (int)val[4];
```
An explicit cast from `long long` to `int`. Safe because `parse_number` already guaranteed the value is at most `INT_MAX`. The cast is written out so a reader can see the narrowing is intentional rather than accidental.

```c
	return (sim->burnout > 0);
```
The one value that cannot be zero. `time_to_burnout = 0` would mean a coder burns out at the instant the simulation starts, before it can physically do anything. The others may legitimately be 0: `compile_ms = 0` means compiling is instantaneous, which is a valid stress test.

This returns the comparison result directly: 1 if positive, 0 otherwise.

---

### `parse_sched()`

**Purpose.** Convert the scheduler word into the internal `mode` number.

```c
	if (!strcmp(str, "fifo"))
		sim->mode = MODE_FIFO;
	else if (!strcmp(str, "edf"))
		sim->mode = MODE_EDF;
	else
		return (0);
```
`strcmp` returns **0 when the strings are equal**, so `!strcmp(a, b)` reads as "a equals b". This trips people up; be ready for the question.

The comparison is exact and case sensitive, which is what the subject demands ("must be exactly one of: fifo or edf"). `"FIFO"`, `"Fifo"`, `"lifo"` and `"edf2"` are all rejected.

`MODE_FIFO` is 0 and `MODE_EDF` is 1, from the header. Storing an `int` rather than the string means the hot path — `heap_less`, called thousands of times — does one integer comparison instead of a string comparison.

---

### `parse_args()`

**Purpose.** The orchestrator. Check the count, then delegate.

```c
	if (ac != 9)
		return (usage_error());
```
Nine, not eight: `av[0]` is the program name plus 8 real arguments. `!=` rather than `<` catches both too few and too many.

```c
	if (!parse_number(av[1], &nb) || nb < 1 || nb > MAX_CODERS)
		return (usage_error());
```
Three conditions joined by `||`, which **short-circuits** — if `parse_number` fails, `nb` is never read, which matters because it would be uninitialised.

- `nb < 1` — zero coders means no simulation. The subject says coders are numbered "from 1 to number_of_coders", so at least one must exist.
- `nb > MAX_CODERS` — 200 is my chosen cap. The subject sets no limit, but each thread costs stack space, and the evaluation scale says "Do not test with more than 200 coders". This is a design decision you can defend or change.

```c
	sim->nb_coders = (int)nb;
	if (!parse_times(sim, av))
		return (usage_error());
	if (!parse_sched(sim, av[8]))
		return (usage_error());
	return (1);
```
The checks are split into separate `if`s rather than combined, purely to stay readable inside the Norm's line limit.

`usage_error()` returns 0, so `return (usage_error())` prints the message and returns failure in one line — a common 42 idiom that keeps functions short.

---

### `usage_error()`

**Purpose.** Print usage to `stderr` and return 0.

```c
	fprintf(stderr, "Usage: ./codexion number_of_coders time_to_burnout "
		"time_to_compile time_to_debug time_to_refactor "
		"number_of_compiles_required dongle_cooldown <fifo|edf>\n"
		"Values must be integers >= 0, time_to_burnout > 0, "
		"number_of_coders between 1 and %d.\n", MAX_CODERS);
```

Two details worth knowing.

**Why `stderr` and not `stdout`.** Error messages belong on the error stream so that `./codexion ... > log.txt` captures only simulation output, leaving errors visible on the terminal.

**Why one call with `%d` instead of several plain calls.** This is the bug I found when checking against the evaluation scale. When `fprintf` is given a format string containing **no** conversion specifiers, gcc rewrites the call into `fwrite` as an optimisation. `fwrite` is **not** on the subject's allowed function list, and `nm -u ./codexion` would show it. Keeping `%d` in the format string blocks that rewrite, so the binary genuinely calls only `fprintf`.

Adjacent string literals in C are automatically joined into one, which is why the message can span five source lines and stay under the 80-column Norm limit while remaining a single call.

---

## Quick self-test on Part 1

**Q1.** Why is `sim` declared inside `main` rather than as a global?

> Globals are forbidden by the subject. Declaring it in `main` and passing `&sim` to every thread gives shared access without a global.

**Q2.** `sim->compile_ms` is read by five threads with no mutex. Why is that not a data race?

> A data race needs at least one writer while others access it. `compile_ms` is written once by `parse_times` before any thread exists and never modified again. Read-only sharing needs no lock.

**Q3.** What breaks if you delete `sim->coders[i].last_compile = sim->start;` from `start_threads`?

> `last_compile` stays 0 from the `memset`. The monitor computes `now_ms() - 0`, about 1.7 trillion ms, which exceeds any burnout value, so every coder is declared burned out on the monitor's first pass, a few hundred microseconds in.

**Q4.** Why does `parse_number` check overflow inside the loop rather than after it?

> Checking after would let a very long input overflow `long long` itself, and signed overflow is undefined behaviour, so the check could not be trusted. Checking per digit caps the value near 21 billion, far from the `long long` limit.

**Q5.** Two threads run `coder->compiles++` — is that the race the `state` mutex prevents in your code?

> No. Only one thread ever writes a given coder's `compiles`. The mutex protects the coder's **write** against the monitor's concurrent **read**, and forces the value to be published to memory rather than cached in a register.

---
---

# PART 2 — Initialisation and the priority queue

---

## 6. Concepts needed before `init.c`

### 6.1 `malloc` and `sizeof`

**Simple version.** `malloc` asks the operating system for a block of memory and hands you its address. You must give it back with `free`.

**Technical version.** `malloc(n)` returns a `void *` to `n` bytes of uninitialised heap memory, or `NULL` if the request cannot be satisfied. The memory contains whatever was there before — never assume zeros.

In your code:

```c
	sim->dongles = malloc(sizeof(t_dongle) * sim->nb_coders);
```

`sizeof(t_dongle)` is the byte size of one dongle struct, computed by the compiler. Multiplying by `nb_coders` requests room for exactly that many. Writing it this way rather than hardcoding a number means the code stays correct if you add a field to `t_dongle` later.

**Why the result must be checked.** If the system is out of memory, `malloc` returns `NULL`, and `sim->dongles[0].taken = 0` would then write to address 0 — an instant segfault. The scale says any segfault is 0 on the project, so every `malloc` in this code is followed by a check.

### 6.2 `memset`

`memset(pointer, value, bytes)` writes the same byte across a region. `memset(sim->dongles, 0, sizeof(t_dongle) * sim->nb_coders)` zeroes the whole array.

This gives every dongle sensible defaults for free:

| Field | After `memset` | Meaning |
|---|---|---|
| `taken` | 0 | Nobody holds it |
| `free_at` | 0 | Cooldown already expired, since `now_ms()` is always far greater than 0 |
| `queue.data` | `NULL` | No array yet, so `free(NULL)` in cleanup is safe |
| `queue.size` | 0 | Queue is empty |

**Order matters.** The `memset` happens **before** `pthread_mutex_init`. A mutex is an opaque struct with internal bookkeeping; zeroing it after initialisation would corrupt it. Always zero first, then initialise.

### 6.3 Initialising a mutex

**Simple version.** A mutex is a lock. Before use, it has to be set up.

**Technical version.** `pthread_mutex_init(&m, NULL)` prepares the lock's internal state. The second argument is an attributes object; `NULL` means default behaviour (a normal, non-recursive, process-private mutex). "Non-recursive" matters: if the same thread locks it twice without unlocking, it deadlocks against itself.

Every initialised mutex must eventually be destroyed with `pthread_mutex_destroy`, which is what `cleanup.c` does.

### 6.4 Condition variables — first look

Full treatment comes in Part 3, but you need the idea now because `init_dongles` creates them.

**Simple version.** A condition variable is a waiting room with a bell. A thread that cannot proceed goes to sleep in the waiting room. When another thread changes something relevant, it rings the bell and the sleepers wake to re-check.

**Why not just loop?** Without it you would write:

```c
while (dongle->taken)
	;               // spin, burning 100% CPU
```

That is **busy-waiting**: the thread consumes a whole CPU core doing nothing. With a condition variable the thread is removed from the scheduler entirely and costs nothing until woken.

`pthread_cond_init(&c, NULL)` prepares one. In your code there is one condition variable **per dongle**, paired with that dongle's mutex, because threads wait for one specific dongle.

### 6.5 The ring topology

The subject says coders sit in a circle: "Coder number 1 sits next to coder number number_of_coders. Any other coder number N sits between coder number N-1 and coder number N+1."

With 4 coders:

```
                  coder1
          D0     /       \     D1
                /         \
          coder4           coder2
                \         /
          D3     \       /     D2
                  coder3
```

Each dongle sits **between** two coders, and each coder can reach exactly the two dongles beside them. Dongle 0 is shared by coder 1 and coder 4. Dongle 1 is shared by coder 1 and coder 2.

This is the source of all contention in the project: **neighbours compete**, non-neighbours do not. Coder 1 and coder 3 share nothing, so they can compile simultaneously. Coder 1 and coder 2 can never compile simultaneously, because both need dongle 1.

---

## 7. `src/init.c`

### Purpose of the file

Allocate the two arrays, prepare every mutex, condition variable and request queue, and wire each coder to its two dongles. After `init_sim` returns 1, the world is fully built and ready for threads.

---

### `init_sim()`

**Purpose.** The entry point of initialisation. Set scalar defaults, create the two global mutexes, then delegate.

**Inputs.** `t_sim *sim` — already filled with parsed arguments.

**Returns.** 1 on success, 0 if any allocation failed.

**Line by line.**

```c
	sim->running = 1;
```
The simulation-alive flag. Set to 1 **before** any thread exists, so no thread can ever observe it as 0 at startup. Every coder loop and `precise_sleep` checks it; the monitor clears it to end the run.

```c
	sim->seq = 0;
	sim->ready = 0;
```
`seq` is the global arrival counter used by FIFO. Every request gets the next number, so "smaller `seq` means arrived earlier" holds across the whole simulation.

`ready` counts how many dongles are **completely** initialised. Its only purpose is safe cleanup after partial failure. If `heap_init` fails on dongle 3 of 5, then `ready == 3`, and `destroy_dongles` destroys exactly 3 mutexes rather than 5. Destroying an uninitialised mutex is undefined behaviour.

```c
	pthread_mutex_init(&sim->state, NULL);
	pthread_mutex_init(&sim->print, NULL);
```
The two program-wide locks. Created here, before the arrays, so they exist on every path that could later call `destroy_sim`.

```c
	if (!init_dongles(sim))
		return (0);
	if (!init_coders(sim))
		return (0);
	return (1);
```
Dongles first, because a coder's `left` and `right` are indices into the dongle array. The order is not strictly required — the indices are just integers — but it keeps the mental model straight: build the table, then seat the people.

On failure it returns 0 immediately, leaving whatever was allocated in place. `main` then calls `destroy_sim`, which frees exactly what exists.

---

### `init_dongles()`

**Purpose.** Allocate the dongle array and prepare each dongle's three components: its request queue, its mutex, its condition variable.

**Variables.** `int i` — index of the dongle being initialised.

**Line by line.**

```c
	sim->dongles = malloc(sizeof(t_dongle) * sim->nb_coders);
	if (!sim->dongles)
		return (0);
	memset(sim->dongles, 0, sizeof(t_dongle) * sim->nb_coders);
```
Number of dongles equals number of coders. The subject states this directly: "There are as many dongles as coders."

```c
	while (i < sim->nb_coders)
	{
		if (!heap_init(&sim->dongles[i].queue, sim->nb_coders, sim->mode))
			return (0);
```
`&sim->dongles[i].queue` is the address of the `t_heap` embedded inside dongle `i`. It is not a pointer field — the heap struct lives **inside** the dongle — but `heap_init` needs an address to write through.

Capacity is `nb_coders`. **Why that is always enough:** a coder can have at most one outstanding request in any given dongle's queue. It pushes once in `take_dongle`, and that entry leaves the queue either by `heap_pop` (granted) or `heap_remove` (cancelled) before the coder can push again. With `nb_coders` coders, at most `nb_coders` entries can exist. This is why the heap never needs to grow, which removes an entire class of reallocation bugs.

`sim->mode` is copied into each heap so that `heap_less` can decide FIFO versus EDF without reaching back to `sim`.

```c
		pthread_mutex_init(&sim->dongles[i].lock, NULL);
		pthread_cond_init(&sim->dongles[i].cond, NULL);
		sim->ready = i + 1;
		i++;
	}
```
Each dongle gets its **own** lock and condition variable. That is a deliberate design decision worth defending: a single global dongle mutex would serialise the entire simulation, so coder 1 taking dongle 0 would block coder 3 taking dongle 2 even though they share nothing. Per-dongle locks mean non-neighbours never contend.

`ready = i + 1` is updated **after** all three succeed, so it always means "this many dongles are fully constructed".

**Order inside the loop matters.** `heap_init` runs first. If it fails, the mutex and condvar for that dongle were never created, and `ready` was not bumped, so cleanup skips them exactly. Had the mutex been created first and the heap second, a failure would leave an initialised mutex that cleanup would never destroy.

**Edge cases.**

| Case | Result |
|---|---|
| `nb_coders == 1` | One dongle, one mutex, one condvar, heap capacity 1. The lone coder can never obtain two dongles |
| `malloc` fails immediately | Returns 0 with `dongles == NULL`; `destroy_dongles` sees `NULL` and returns without touching anything |
| `heap_init` fails at `i == 2` | `ready == 2`; cleanup destroys 2 mutexes and frees the array |

---

### `init_coders()`

**Purpose.** Allocate the coder array and give each coder its identity and its two dongle indices.

```c
		sim->coders[i].id = i + 1;
```
Array indices run 0..n-1, but the subject requires coder numbers 1..n in the output. Storing `id` separately keeps the printed number correct while the array stays 0-based.

```c
		sim->coders[i].left = i;
		sim->coders[i].right = (i + 1) % sim->nb_coders;
```
This is the ring. `left` is simply the coder's own index. `right` is the next index, wrapped by the modulo.

**What `%` does here.** For 4 coders:

| Coder index `i` | `id` | `left = i` | `right = (i+1) % 4` |
|---|---|---|---|
| 0 | 1 | 0 | 1 |
| 1 | 2 | 1 | 2 |
| 2 | 3 | 2 | 3 |
| 3 | 4 | 3 | **0** ← wraps |

Without the modulo, coder 4 would have `right == 4`, which is out of bounds on a 4-element array — a read past the end of the heap block, which valgrind would flag as an invalid read and which could segfault.

The wrap is what closes the circle, satisfying "Coder number 1 sits next to coder number number_of_coders".

```c
		sim->coders[i].sim = sim;
```
The back-pointer. This single line is why `coder_routine` needs only one argument: from its own `t_coder`, a thread can reach the dongles, the mutexes and every parameter through `coder->sim->...`.

Note `compiles` and `last_compile` are left at 0 from the `memset`. `last_compile` is filled later by `start_threads`, at the moment the clock starts — not here, because `init_sim` may run measurably before the simulation begins.

**Test yourself.** *With `nb_coders == 2`, what are the dongles of each coder?*

> Coder 1: `left = 0`, `right = 1`. Coder 2: `left = 1`, `right = (1+1) % 2 = 0`. Both coders need **both** dongles, so only one can compile at a time. This is why a 2-coder run shows strictly alternating compiles, never overlapping.

---

## 8. Concepts: priority queue and binary heap

### 8.1 Why a plain queue is not enough

**Simple version.** A normal queue serves people in arrival order. Sometimes you need to serve the most urgent person first instead.

Under FIFO, arrival order is exactly what you want. But under EDF the dongle must go to the coder closest to burning out, who may have arrived last. A plain list cannot do both.

A **priority queue** is a structure that always hands you the highest-priority item, whatever order things arrived in. "Priority" is defined by a comparison function — and that is the trick your code uses: one structure, and swapping the comparison switches the entire policy.

The subject requires you build it yourself: "You must implement a priority queue (heap) for FIFO/EDF scheduling (no standard library priority queue may be used)."

### 8.2 What a binary heap is

**Simple version.** A tree where every parent is more important than its children. The most important item is always at the top, so you can grab it instantly.

**Technical version.** A binary heap is a **complete binary tree** (every level full except possibly the last, which fills left to right) satisfying the **heap property**: for every node, `parent` is not-less-important than its children.

Note the heap property says nothing about siblings. `data[1]` and `data[2]` can be in any order relative to each other. The **only** guarantee is that the root is the best element. That is all we need, because `take_dongle` only ever asks "am I at the root?"

### 8.3 The array trick

A binary heap needs no pointers and no allocation per node. The tree is stored in a flat array, with position determining the parent/child relationship:

```
index:     0       1       2       3       4       5       6
        ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐
        │  A   │  B   │  C   │  D   │  E   │  F   │  G   │
        └──────┴──────┴──────┴──────┴──────┴──────┴──────┘

as a tree:
                    A (0)
                  /        \
              B (1)         C (2)
             /     \       /     \
         D (3)   E (4)  F (5)   G (6)
```

The arithmetic:

| Relationship | Formula | Example |
|---|---|---|
| Left child of `i` | `2*i + 1` | left child of 1 is 3 |
| Right child of `i` | `2*i + 2` | right child of 1 is 4 |
| Parent of `i` | `(i - 1) / 2` | parent of 4 is `3/2 = 1` (integer division) |

Integer division is what makes the parent formula work for both children: parent of 3 is `2/2 = 1`, parent of 4 is `3/2 = 1`. Both children map to the same parent.

**Why this is better than a sorted list.** Inserting into a sorted array means shifting elements: O(n). A heap only walks one path from a leaf to the root: O(log n). With 200 coders, that is about 8 comparisons instead of up to 200. It also never allocates a node, so there is no per-request `malloc` in the hot path and nothing extra to leak.

---

## 9. `src/heap.c`

### Purpose of the file

The five public operations on a request queue: create, destroy, insert, remove-best, look-at-best. It knows nothing about scheduling policy — that lives entirely in `heap_less`.

---

### `heap_init()`

**Purpose.** Allocate the request array and set the heap to empty.

**Inputs.** `t_heap *heap`, `int capacity`, `int mode`.

**Returns.** 1 on success, 0 if `malloc` failed.

```c
	heap->data = malloc(sizeof(t_request) * capacity);
	if (!heap->data)
		return (0);
	memset(heap->data, 0, sizeof(t_request) * capacity);
	heap->size = 0;
	heap->capacity = capacity;
	heap->mode = mode;
	return (1);
```

**Variables of `t_heap`:**

| Field | Type | Meaning | Changes when |
|---|---|---|---|
| `data` | `t_request *` | Address of the array of requests | Once at init, once at free |
| `size` | `int` | How many requests are currently queued | Every push, pop, remove |
| `capacity` | `int` | How many fit | Never after init |
| `mode` | `int` | `MODE_FIFO` or `MODE_EDF` | Never after init |

The `memset` is not strictly required, since `size = 0` means nothing is read. It exists so that if you inspect the array in a debugger, you see zeros rather than noise.

The distinction between `size` and `capacity` is the whole safety mechanism: `capacity` is the physical array length, `size` is how much is in use. Reading `data[size]` when `size < capacity` is legal memory but meaningless data; reading `data[capacity]` is out of bounds.

---

### `heap_push()`

**Purpose.** Insert one request while keeping the heap property intact.

**Returns.** 1 on success, 0 if the heap is full.

```c
	if (heap->size >= heap->capacity)
		return (0);
```
**What is checked:** is there room? **If true** (full), reject — writing `data[capacity]` would corrupt memory past the block.

**Can this happen in practice?** No, as argued earlier: capacity equals `nb_coders` and each coder can queue at most one request per dongle. The check is defensive. It also gives `take_dongle` something honest to do on the impossible path: unlock and report failure rather than continue with a corrupted queue.

```c
	heap->data[heap->size] = req;
	heap->size++;
	sift_up(heap, heap->size - 1);
	return (1);
```
The new request is appended at the first free slot, which is the next position in the complete tree. That may violate the heap property — the new item might outrank its parent — so `sift_up` repairs it by walking upward.

`heap->size - 1` is the index just written, because `size` was already incremented.

**Note this is pass-by-value:** the parameter is `t_request req`, not `t_request *req`. The whole 24-byte struct is copied into the array. That is deliberate — the caller's `req` lives on its stack and disappears when `take_dongle` returns, so storing a pointer to it would leave a dangling reference.

---

### `heap_pop()`

**Purpose.** Remove the root, the highest-priority request.

```c
	if (heap->size == 0)
		return (0);
	heap->size--;
	heap->data[0] = heap->data[heap->size];
	sift_down(heap, 0);
	return (1);
```

This is the clever part. You cannot just delete `data[0]` — that would leave a hole at the top of the tree. Instead:

1. `size--` logically drops the **last** element from the tree.
2. That last element is copied over the root, filling the hole.
3. The root is now probably wrong (a leaf is rarely the best element), so `sift_down` pushes it downward until the heap property holds.

The tree stays complete because we removed the last slot, which is exactly the one the completeness rule says must go.

Note it does **not** return the popped request. `take_dongle` always calls `heap_peek` first to check identity, so it already knows what is at the root. Returning nothing keeps the function short and avoids the question of returning a pointer into an array that is about to be rearranged.

---

### `heap_peek()`

**Purpose.** Look at the best request without removing it.

```c
	if (heap->size == 0)
		return (NULL);
	return (&heap->data[0]);
```

Returns a **pointer into the array**, not a copy. That is safe here because every caller uses it immediately while holding the dongle's mutex. If you stored that pointer and used it later, a push or pop could have moved that slot's contents and the pointer would refer to a different request.

`NULL` on empty is what lets `dongle_ready` write `if (!top) return (0);` — an empty queue means nobody is waiting, so nobody can be granted the dongle.

---

### `heap_free()`

```c
	free(heap->data);
	heap->data = NULL;
	heap->size = 0;
	heap->capacity = 0;
```

`free` releases the block. Setting `data = NULL` afterwards prevents a **double free** and a **use-after-free**: if the function were somehow called twice, the second `free(NULL)` does nothing. Zeroing `size` and `capacity` means any accidental later use finds an empty heap rather than a stale one.

---

## 10. `src/heap_utils.c`

### `heap_less()` — the entire scheduling policy

**Purpose.** Answer one question: should request `a` be served before request `b`?

**Returns.** 1 if `a` has higher priority, 0 otherwise.

```c
	if (heap->mode == MODE_EDF)
	{
		if (a->deadline != b->deadline)
			return (a->deadline < b->deadline);
		return (a->id < b->id);
	}
	return (a->seq < b->seq);
```

This is the most important function in the project to be able to explain. Everything about scheduling lives in these five lines; the heap machinery is policy-blind.

**The FIFO branch** — `return (a->seq < b->seq);`

`seq` comes from `sim->seq`, a counter incremented once per request in `build_request`. Request number 12 arrived before request number 15, always. Smaller `seq` wins, so the earliest arrival sits at the root. That is First In, First Out exactly as the subject defines it: "the dongle is granted to the coder whose request arrived first".

**The EDF branch** — deadline first.

`deadline` is `last_compile + burnout`, computed in `build_request`. It is the wall-clock moment this coder will burn out if it does not compile. Smaller deadline means closer to death, so it wins. The subject: "serve the coder with the earliest burnout deadline (i.e., `last_compile_start + time_to_burnout`)".

**The tie-break** — `return (a->id < b->id);`

If two coders share a deadline to the millisecond, the lower ID wins. Without this the comparison would return 0 in both directions (`a` not before `b`, `b` not before `a`), and the ordering would depend on incidental array positions — the result could differ between runs with identical input.

The subject asks for exactly this: "The tie-breaker rule is required to ensure a fully deterministic EDF policy, even in edge cases."

**When do ties actually happen?** More often than you might guess, and this is a good defense detail. At `t = 0`, every coder has `last_compile == sim->start`, so **every** coder's first deadline is identical. The tie-break decides the entire opening round, which is why low-numbered coders tend to compile first at startup.

---

### `heap_swap()`

```c
	tmp = *a;
	*a = *b;
	*b = tmp;
```

The three-line exchange. `*a` means "the struct at address a", so this copies whole `t_request` values, all three fields at once. `tmp` is a full struct on the stack, not a pointer.

Why a helper instead of writing it inline? `sift_up` and `sift_down` both need it, and the Norm's 25-line limit makes shared helpers necessary rather than optional.

---

### `sift_up()` — repair after an insert

**Purpose.** A new item was placed at the bottom. Move it up until its parent outranks it.

```c
	while (i > 0)
	{
		parent = (i - 1) / 2;
		if (!heap_less(heap, &heap->data[i], &heap->data[parent]))
			return ;
		heap_swap(&heap->data[i], &heap->data[parent]);
		i = parent;
	}
```

**`while (i > 0)`** — index 0 is the root, which has no parent. Reaching it means we are done.

**The `if`** — "if this item is **not** better than its parent, stop". The heap property holds from here up, because the parent was already correctly placed relative to everything above it.

**If false** (the item *is* better), swap it with its parent and continue from the parent's position.

**Worked trace, EDF mode.** Push four requests in this order:

| Push | id | seq | deadline |
|---|---|---|---|
| 1st | 1 | 10 | 900 |
| 2nd | 2 | 20 | 300 |
| 3rd | 3 | 30 | 700 |
| 4th | 4 | 40 | 100 |

```
push id1(900):  data = [900]                       size=1
                sift_up(0): i == 0, loop never runs

push id2(300):  data = [900, 300]                  size=2
                sift_up(1): parent = (1-1)/2 = 0
                            300 < 900 ? yes -> swap
                data = [300, 900]
                            i = 0, loop ends

push id3(700):  data = [300, 900, 700]             size=3
                sift_up(2): parent = (2-1)/2 = 0
                            700 < 300 ? no -> return
                data unchanged

                        300(id2)
                       /        \
                  900(id1)     700(id3)

push id4(100):  data = [300, 900, 700, 100]        size=4
                sift_up(3): parent = (3-1)/2 = 1
                            100 < 900 ? yes -> swap
                data = [300, 100, 700, 900]
                            i = 1
                            parent = (1-1)/2 = 0
                            100 < 300 ? yes -> swap
                data = [100, 300, 700, 900]
                            i = 0, loop ends

                        100(id4)
                       /        \
                  300(id2)     700(id3)
                  /
             900(id1)
```

The root is now id4 — the coder closest to burnout. Four pushes cost at most 2 comparisons each on a 4-element heap.

---

### `sift_down()` — repair after a pop

**Purpose.** The root was replaced by a leaf. Push it down until both children are worse than it.

```c
	child = 2 * i + 1;
	while (child < heap->size)
	{
		if (child + 1 < heap->size
			&& heap_less(heap, &heap->data[child + 1], &heap->data[child]))
			child++;
		if (!heap_less(heap, &heap->data[child], &heap->data[i]))
			return ;
		heap_swap(&heap->data[child], &heap->data[i]);
		i = child;
		child = 2 * i + 1;
	}
```

**`while (child < heap->size)`** — if the left child index is past the end, this node is a leaf and there is nothing below to compare with.

**The first `if` — choosing which child.**

`child + 1 < heap->size` asks "does a right child exist?" — checked **first**, because `&&` short-circuits. If there is no right child, `heap_less` is never called on `data[child + 1]`, which would be out of bounds. Reversing these two operands would produce an invalid read that valgrind flags and that can crash.

If a right child exists and it is better than the left, `child++` selects it. We must descend toward the **better** of the two children; swapping with the worse one could leave that better child above its new parent, breaking the heap property.

**The second `if` — stop condition.** If the best child is not better than the current node, the heap property holds here and below, so return.

**Worked trace, continuing the heap above.** `heap_pop` on `[100, 300, 700, 900]`:

```
step 1: size-- -> 3
step 2: data[0] = data[3]  (the last element, 900, moves to the root)
        data = [900, 300, 700]     size=3

                        900(id1)      <- clearly wrong
                       /        \
                  300(id2)     700(id3)

step 3: sift_down(0)
        child = 2*0+1 = 1
        child+1 = 2 < 3 ? yes, and is data[2]=700 better than data[1]=300?
                  700 < 300 ? no  -> keep child = 1
        is data[1]=300 better than data[0]=900?
                  300 < 900 ? yes -> swap
        data = [300, 900, 700]
        i = 1, child = 2*1+1 = 3, and 3 < 3 is false -> loop ends

                        300(id2)
                       /        \
                  900(id1)     700(id3)
```

Root is id2, the next-earliest deadline. Popping the rest yields id3 (700) then id1 (900). Full grant order: **4, 2, 3, 1** — by deadline, not by arrival. Under FIFO mode with the same four requests the order would be 1, 2, 3, 4, by `seq`.

That contrast is the single clearest way to demonstrate the two policies at defense.

---

### `heap_remove()` — cancelling a request

**Purpose.** Remove a specific coder's request from anywhere in the heap, not just the root.

```c
	i = 0;
	while (i < heap->size)
	{
		if (heap->data[i].id == id)
		{
			heap->size--;
			heap->data[i] = heap->data[heap->size];
			sift_down(heap, i);
			sift_up(heap, i);
			return (1);
		}
		i++;
	}
	return (0);
```

**Why it exists.** When the simulation ends while a coder is queued and waiting, that coder gives up. Its request must leave the queue. If it stayed, it would sit at the root forever, and any other coder still waiting would check "am I at the root?", see a ghost request from a thread that has already exited, and wait forever. `main` would then hang in `pthread_join`.

**The linear scan.** Unlike push and pop, this is O(n): the target can be anywhere. That is acceptable because it runs at most once per coder per dongle, only during shutdown, never in the hot path.

**Why both `sift_down` and `sift_up`.** The replacement element came from the end of the array and can be either better or worse than what sat at position `i`.

- Worse than its children → `sift_down` moves it down, `sift_up` then does nothing.
- Better than its parent → `sift_down` does nothing, `sift_up` moves it up.

Calling both is simpler and cheaper than working out which case applies, and exactly one of them ever does work.

**Returns 0 if not found**, which is a legitimate outcome: the request may have already been popped by a grant that happened microseconds earlier.

---

## 11. How these connect

```
take_dongle()                         [dongle.c]
    │
    ├── build_request()   makes {id, seq, deadline}
    │
    ├── heap_push()       [heap.c]
    │       └── sift_up()      [heap_utils.c]
    │               └── heap_less()   <- FIFO or EDF decided here
    │                       heap_swap()
    │
    ├── wait_for_turn()
    │       └── dongle_ready()
    │               └── heap_peek()   [heap.c]   "am I at the root?"
    │
    ├── heap_pop()        [heap.c]    granted: remove myself from the queue
    │       └── sift_down()   [heap_utils.c]
    │               └── heap_less()
    │
    └── heap_remove()     [heap_utils.c]  cancelled: remove myself anyway
            └── sift_down() + sift_up()
```

Every one of these heap calls happens while the calling thread holds that dongle's `lock`. The heap itself contains **no** locking. That is a design decision you should be able to state plainly: the queue is a plain data structure, and the mutex that protects it belongs to the dongle that owns it. Mixing locking into the data structure would mean locking twice, or locking at the wrong granularity.

---

## Quick self-test on Part 2

**Q1.** Why is heap capacity `nb_coders` rather than something larger?

> A coder can have at most one outstanding request per dongle — it pushes once in `take_dongle` and that entry is removed by either `heap_pop` or `heap_remove` before it can push again. With `nb_coders` coders, `nb_coders` slots always suffice, so the heap never needs to grow.

**Q2.** In `sift_down`, why must `child + 1 < heap->size` be tested before calling `heap_less` on `data[child + 1]`?

> Because `&&` short-circuits. If the right child does not exist, the bounds test fails and `heap_less` is never called, so the out-of-bounds element is never read. Swapping the order would produce an invalid read past the live part of the array.

**Q3.** Coder 2 and coder 5 both have deadline 3400. Under EDF, who gets the dongle?

> Coder 2. The deadlines are equal, so `heap_less` falls through to `return (a->id < b->id)` and the lower ID wins. This makes the policy deterministic instead of dependent on array layout.

**Q4.** What would break if `heap_remove` did not exist and a cancelled request stayed in the queue?

> The dead coder's request could sit at the root permanently. `dongle_ready` would keep answering "the root is not you" to every remaining waiter, so nobody could ever take that dongle, and `main` would hang forever in `pthread_join`.

**Q5.** `heap_peek` returns a pointer into the array. Why is that not dangerous here?

> Every caller dereferences it immediately while holding the dongle's mutex, so no push or pop can rearrange the array in between. Storing that pointer and using it after a later push or pop would be a bug, because the slot's contents would have moved.

---
---

# PART 3 — Dongle arbitration, the heart of the project

This is the part evaluators dig into. Take it slowly.

---

## 12. Concepts before `dongle.c`

### 12.1 Mutexes, properly

**Simple version.** A mutex is a key to a room. Only one thread can hold the key. Anyone else who wants in must wait at the door until the key is returned.

**Technical version.** A mutex (mutual exclusion lock) has two states, locked and unlocked, plus a queue of waiting threads.

- `pthread_mutex_lock(&m)` — if unlocked, lock it and continue. If already locked, **block** the calling thread until it becomes available.
- `pthread_mutex_unlock(&m)` — release it and wake one waiter.

The region between lock and unlock is a **critical section**. Code inside it runs without interference on the data that mutex protects.

Three properties matter:

1. **Blocking is free.** A thread waiting on a mutex is removed from the CPU entirely. It costs nothing until woken.
2. **Non-recursive by default.** Locking a mutex you already hold deadlocks you against yourself, instantly and permanently.
3. **It protects data, not code.** A mutex has no idea what it guards. The guarantee only exists because *every* access to that data goes through *that* mutex. One unprotected access anywhere destroys the guarantee.

**In your project:**

| Mutex | Protects | Locked in | Unlocked in | Why needed |
|---|---|---|---|---|
| `dongle[i].lock` | `taken`, `free_at`, `queue` of dongle `i` | `take_dongle`, `drop_dongle`, `stop_sim` | same functions | Stops two coders being handed the same dongle; keeps the heap from being rearranged mid-read |
| `sim->state` | `running`, `seq`, every coder's `compiles` and `last_compile` | `build_request`, `do_compile`, `coder_done`, `sim_running`, `stop_sim`, `check_burnout`, `all_done`, `log_state` | same | Lets the monitor read coder progress without tearing; publishes the stop flag |
| `sim->print` | `stdout` | `log_state`, `log_burnout` | same | Stops two messages interleaving on one line |

### 12.2 Condition variables and `pthread_cond_timedwait`

**Simple version.** A mutex makes you wait for a *key*. A condition variable makes you wait for a *situation* — "wake me when something changes".

**Technical version.** A condition variable is always paired with a mutex. The core call:

```c
pthread_cond_timedwait(&dongle->cond, &dongle->lock, &ts);
```

does three things **atomically**:

1. Unlocks `dongle->lock`.
2. Puts this thread to sleep on `dongle->cond`.
3. When woken, or when the absolute time `ts` passes, **re-locks** `dongle->lock` before returning.

The atomicity of steps 1 and 2 is the entire reason condition variables exist. If you unlocked and then slept as two separate operations, another thread could sneak in between, change the state, and signal — and you would go to sleep after the signal already fired, missing it forever. That is the **lost wakeup** problem.

Two ways to wake a sleeper:

- `pthread_cond_signal(&c)` — wakes one arbitrary waiter.
- `pthread_cond_broadcast(&c)` — wakes **all** waiters.

Your code always broadcasts. Reason: the waiters are not interchangeable. Only one specific coder — the one at the heap root — can proceed. `signal` might wake the wrong one, who checks, sees it is not its turn, and goes back to sleep while the right coder sleeps on. Broadcast wakes everyone, each re-checks, exactly one proceeds.

**The golden rule: always wait in a loop, never in an `if`.** Three reasons:

1. **Spurious wakeups.** POSIX permits `pthread_cond_wait` to return without any signal at all. This is not a bug, it is allowed by the standard.
2. **Stolen conditions.** Between the broadcast and your thread actually running, another thread may have already taken what you were waiting for.
3. **Timeouts.** `timedwait` returns when the deadline passes, with nothing having changed.

In all three cases the correct response is identical: re-check the condition, and if it still does not hold, wait again. Your `wait_for_turn` is written exactly this way.

### 12.3 Deadlock and Coffman's four conditions

**Simple version.** Two threads each hold something the other needs, and neither will let go. Both wait forever.

The classic dining-philosophers deadlock, in your vocabulary:

```
t=0   Coder 1 takes dongle 0     Coder 2 takes dongle 1
t=1   Coder 1 wants dongle 1     Coder 2 wants dongle 2
      (held by coder 2)          (held by coder 3)
t=1   Coder 3 takes dongle 2     Coder 4 takes dongle 3
t=1   Coder 3 wants dongle 3     Coder 4 wants dongle 0
      (held by coder 4)          (held by coder 1)

Every coder holds one dongle and waits for one held by a neighbour.
The wait-for graph is a closed circle. Nobody ever moves again.
Output stops. The program hangs. pthread_join never returns.
```

**Technical version.** Coffman proved deadlock requires **all four** of these simultaneously. Break any one and deadlock becomes impossible:

| Condition | Meaning |
|---|---|
| 1. Mutual exclusion | A resource cannot be shared |
| 2. Hold and wait | A thread holding one resource can request another |
| 3. No preemption | A resource cannot be forcibly taken back |
| 4. Circular wait | A cycle exists in the wait-for graph |

**Your project's answer**, which you must be able to recite:

| Condition | Status in Codexion | Reasoning |
|---|---|---|
| Mutual exclusion | **kept** | A dongle is exclusive by definition — that is the exercise |
| Hold and wait | **kept** | `take_two` holds the first dongle while queueing for the second |
| No preemption | **kept** | A dongle is never taken back from its holder |
| Circular wait | **broken** | Odd coders take left-then-right, even coders take right-then-left |

**Why breaking the cycle works.** In `take_two`:

```c
	if (coder->id % 2 == 0)
	{
		first = &coder->sim->dongles[coder->right];
		second = &coder->sim->dongles[coder->left];
	}
```

With 4 coders the request orders become:

| Coder | id | left | right | parity | Takes first | Takes second |
|---|---|---|---|---|---|---|
| 1 | 1 | 0 | 1 | odd | **0** | 1 |
| 2 | 2 | 1 | 2 | even | **2** | 1 |
| 3 | 3 | 2 | 3 | odd | **2** | 3 |
| 4 | 4 | 3 | 0 | even | **0** | 3 |

Replay the deadlock attempt:

```
t=0   Coder 1 takes dongle 0.   Coder 2 takes dongle 2.
t=0   Coder 3 wants dongle 2 -> already taken by coder 2, so coder 3
                                 waits holding NOTHING.
t=0   Coder 4 wants dongle 0 -> already taken by coder 1, so coder 4
                                 waits holding NOTHING.
t=0   Coder 1 wants dongle 1 -> free -> takes it -> COMPILES.
t=0   Coder 2 wants dongle 1 -> taken by coder 1, waits.

No cycle. Coder 1 makes progress, finishes, releases both.
```

The key insight: the deadlock needs *every* coder to grab one dongle before *any* coder grabs a second. Parity-based ordering makes two neighbours compete for the *same* first dongle, so one of them is stopped before it holds anything. A thread waiting while holding nothing can never be part of a cycle.

**Second layer of protection: lock ordering.** The dongle argument above is about the simulated resource. There is a separate question about the mutexes themselves — two threads could deadlock on `state` and `lock` if they acquired them in opposite orders. Your code has exactly two nesting orders anywhere in the program:

```
dongle->lock  THEN  sim->state     (build_request, sim_running inside wait_for_turn)
sim->print    THEN  sim->state     (log_state)
```

The reverse orders never occur. Nothing takes a dongle lock while holding `state`; nothing takes `print` while holding `state`. Since no two threads can ever want the same pair in opposite orders, the mutexes cannot deadlock. This is a **lock hierarchy**, and it is independent of the dongle-ordering argument.

Notice `take_dongle` calls `log_state` **after** unlocking the dongle:

```c
	pthread_mutex_unlock(&dongle->lock);
	if (granted)
		log_state(coder, "has taken a dongle");
```

Had the log call been inside the critical section, you would have `dongle->lock → print → state`, introducing a third order and a real risk of violating the hierarchy later. It also keeps the dongle lock held for the shortest possible time, since `printf` is slow.

---

## 13. `src/dongle.c`

### Purpose of the file

Everything about acquiring and releasing **one** dongle: building a request, deciding whether a dongle can be granted, waiting for your turn, and releasing with cooldown. This is where the subject's "fair arbitration is mandatory" requirement is implemented.

---

### `build_request()`

**Purpose.** Create the `t_request` describing this coder's claim, stamped with both scheduling keys.

**Inputs.** `t_coder *coder`.

**Returns.** A `t_request` **by value** — a copy, not a pointer.

```c
	pthread_mutex_lock(&coder->sim->state);
	coder->sim->seq++;
	req.seq = coder->sim->seq;
	req.deadline = coder->last_compile + coder->sim->burnout;
	pthread_mutex_unlock(&coder->sim->state);
	req.id = coder->id;
	return (req);
```

**Why `state` is locked here.** Two reasons, both real:

`sim->seq++` is a read-modify-write on data shared by **every** coder thread. This is the textbook race — and unlike `compiles`, here there genuinely are multiple writers.

**Simulation without the mutex:**

```
Coder 1 thread                    Coder 3 thread
--------------                    --------------
reads sim->seq   -> 41
                                  reads sim->seq   -> 41
computes 41+1    -> 42
                                  computes 41+1    -> 42
writes sim->seq  = 42
                                  writes sim->seq  = 42
req.seq = 42                      req.seq = 42
```

Both requests now carry `seq == 42`. FIFO ordering between them becomes undefined — `heap_less` returns 0 in both directions, so which one sits at the root depends on array positions. Two coders could be considered equally first, and the counter has advanced by 1 instead of 2, so this collision repeats. Fair arbitration is silently broken, and nothing crashes, so you would never notice without careful log analysis.

The second reason: `coder->last_compile` is written by this coder in `do_compile` under `state` and read by the monitor under `state`. Reading it here under the same mutex keeps that discipline complete.

**Why `req.id = coder->id;` sits outside the lock.** `id` is written once in `init_coders`, before any thread exists, and never modified again. Read-only data needs no protection. Keeping it outside shortens the critical section.

**The deadline, explained.** `last_compile + burnout` is a wall-clock instant in milliseconds since the epoch — the moment this coder dies if it has not started compiling. A coder that compiled long ago has a small (early) deadline and therefore high EDF priority.

**Crucial property:** the deadline does **not** change while the coder waits. It is anchored to the *last* compile, not to the current time. That is what makes EDF work — a coder that has been waiting a long time keeps its early deadline and keeps outranking coders who have recently compiled.

**Edge case at startup:** every coder's `last_compile` equals `sim->start`, so at `t = 0` every deadline is identical and the `id` tie-break decides the whole first round.

---

### `dongle_ready()`

**Purpose.** Answer one question: right now, may **this** coder take **this** dongle?

**Returns.** 1 if all three conditions hold, 0 otherwise.

```c
	if (dongle->taken)
		return (0);
	if (now_ms() < dongle->free_at)
		return (0);
	top = heap_peek(&dongle->queue);
	if (!top)
		return (0);
	return (top->id == coder->id);
```

Three gates, each enforcing a different subject requirement.

**Gate 1 — `if (dongle->taken)`**

*What is checked:* is someone holding it? *If true:* refuse. *What it prevents:* dongle duplication — the same dongle in two coders' hands. The scale checks this explicitly: "Confirm the same dongle is never held by two coders at the same time."

*Concrete example:* coder 1 holds dongle 1 and is compiling. Coder 2 wakes up, calls `dongle_ready`, sees `taken == 1`, returns 0, goes back to sleep.

**Gate 2 — `if (now_ms() < dongle->free_at)`**

*What is checked:* has the cooldown expired? `free_at` was set to `now + cooldown` when the dongle was released. *If true* (current time is still before that instant): refuse.

*Concrete example with `dongle_cooldown = 400`:* coder 1 releases dongle 1 at `t = 1200`, so `free_at = 1600`. Coder 2 is at the root of the queue and wakes at `t = 1250`. `1250 < 1600`, so it is refused and sleeps again. At `t = 1600` the check passes and it is granted.

*What it prevents:* violating "after a coder releases a dongle, the dongle cannot be taken again until `dongle_cooldown` milliseconds have elapsed".

*Design note:* the cooldown is enforced **at the moment of granting**, by the dongle itself. The alternative — asking the releasing coder to sleep for the cooldown — would be wrong, because the cooldown belongs to the dongle, not to the coder, and the coder should be free to go debug immediately.

**Gate 3 — `top->id == coder->id`**

*What is checked:* am I at the front of this dongle's queue? *If false:* refuse even though the dongle is free and cooled.

This is the **no-barging rule**, and it is the single most important line for fairness. Without it, whichever thread the OS happened to wake first would take a free dongle, and the scheduler would be meaningless — arbitration would be "whoever wins the race", not FIFO or EDF.

*Concrete example, FIFO:* dongle 2 is free. Coder 3 queued at `seq = 40`, coder 2 queued at `seq = 55`. Both wake. Coder 2 calls `dongle_ready`, `heap_peek` returns coder 3's request, `3 != 2`, refused. Coder 3 checks, sees itself at the root, takes it. Arrival order is honoured even though coder 2 woke first.

`if (!top)` guards the empty queue. It cannot happen from inside `take_dongle`, which pushed before waiting, but `heap_peek` can return `NULL` and dereferencing it would segfault. Defensive and cheap.

**Important: this function does no locking.** It reads `taken`, `free_at` and the queue — all protected by `dongle->lock`. It is only ever called from `wait_for_turn`, which runs with that lock held. Locking inside would be a double-lock on a non-recursive mutex: instant self-deadlock.

---

### `wait_for_turn()`

**Purpose.** Sleep until this coder may take this dongle, or until the simulation ends.

**Returns.** 1 if the dongle may now be taken, 0 if the simulation ended first.

**Precondition:** the caller holds `dongle->lock`. **Postcondition:** the caller still holds it.

```c
	while (!dongle_ready(dongle, coder))
	{
		if (!sim_running(coder->sim))
			return (0);
		ms_to_timespec(now_ms() + 1, &ts);
		pthread_cond_timedwait(&dongle->cond, &dongle->lock, &ts);
	}
	return (1);
```

**The `while`, not `if`.** As established: spurious wakeups, stolen conditions, and timeouts all mean "woken but maybe not ready". The loop re-checks every time. This is the single most common mistake in threaded code, and writing `if` here would produce a program that works in testing and fails under load.

**The escape hatch.** `if (!sim_running(...)) return (0);` is checked **inside** the loop, on every wakeup. Without it, a coder queued for a dongle that will never be released would wait forever, `main` would block in `pthread_join`, and the program would hang instead of exiting.

Note `sim_running` locks `sim->state` while this thread holds `dongle->lock`. That is the `dongle → state` order from the hierarchy — consistent everywhere.

**The 1 ms tick.** `ms_to_timespec(now_ms() + 1, &ts)` builds an absolute wake-up time one millisecond from now. `pthread_cond_timedwait` returns at the earliest of: a broadcast, or that instant.

Why a timeout rather than a plain `pthread_cond_wait`? Because of gate 2. Consider: the dongle is free, you are at the root, but the cooldown has 300 ms left. Nobody will broadcast when a cooldown expires — a cooldown is the passage of time, not an event any thread performs. With a plain `cond_wait` you would sleep until some unrelated broadcast arrived, which might be much later or never. The timeout guarantees you re-check at least every millisecond, so you take the dongle within 1 ms of the cooldown ending.

It is also insurance: even if a broadcast were somehow missed, the worst outcome is a 1 ms delay rather than a permanent hang.

**Why 1 ms and not, say, 100 microseconds?** The subject's timing granularity is milliseconds and all logged timestamps are integers in milliseconds. Re-checking more often than the unit of measurement would burn CPU for no observable benefit.

**This is not busy-waiting.** Between checks the thread is genuinely asleep and off the CPU. The measured cost is one wake-up per waiting thread per millisecond, which is negligible.

---

### `take_dongle()`

**Purpose.** The full acquisition protocol for one dongle: queue up, wait your turn, take it, announce it.

**Returns.** 1 if the dongle was acquired, 0 if the simulation ended first.

```c
	pthread_mutex_lock(&dongle->lock);
	if (!heap_push(&dongle->queue, build_request(coder)))
	{
		pthread_mutex_unlock(&dongle->lock);
		return (0);
	}
	granted = wait_for_turn(coder, dongle);
	if (granted)
	{
		heap_pop(&dongle->queue);
		dongle->taken = 1;
	}
	else
		heap_remove(&dongle->queue, coder->id);
	pthread_cond_broadcast(&dongle->cond);
	pthread_mutex_unlock(&dongle->lock);
	if (granted)
		log_state(coder, "has taken a dongle");
	return (granted);
```

**Line by line.**

`pthread_mutex_lock(&dongle->lock);` — from here until the unlock, this thread has exclusive rights to this dongle's `taken`, `free_at` and `queue`. Note the lock is held **across the wait**, but `pthread_cond_timedwait` releases it while sleeping, so other threads can still get in.

`heap_push(&dongle->queue, build_request(coder))` — register the claim **before** waiting. Order matters enormously: if you waited first and queued second, you would be waiting for a turn you had never asked for, and `dongle_ready` would never see you at the root. On the failure path (heap full, theoretically impossible) the lock is released before returning, which is the kind of detail that causes hangs when forgotten.

`granted = wait_for_turn(coder, dongle);` — blocks here for most of the function's life.

**The granted branch:**

```c
		heap_pop(&dongle->queue);
		dongle->taken = 1;
```

Pop removes your own request — you are the root, that is exactly what `dongle_ready` verified. Then mark the dongle as held. These two happen with the lock held and with no wait between them, so the window where you are simultaneously at the root *and* the dongle is untaken is closed atomically. Another coder can never observe an inconsistent state.

**The cancelled branch:** `heap_remove(&dongle->queue, coder->id);` takes your request out of wherever it sits. This is the shutdown path discussed in Part 2. Skipping it would leave a ghost request that blocks the queue forever.

`pthread_cond_broadcast(&dongle->cond);` — wake everyone to re-check. Two situations need this. On the granted path, the queue changed, so the next-best waiter's position moved. On the cancelled path, removing a request may have promoted someone else to the root; without a broadcast they might sleep up to 1 ms longer, or, if the tick were removed, forever.

`pthread_mutex_unlock(&dongle->lock);` then `log_state(...)` — logging outside the critical section, for the lock-hierarchy reason given earlier.

**Why log only when granted?** A coder that failed did not take anything, and the simulation has ended anyway. Printing would add a line after the final message.

---

### Step-by-step simulation: two coders, one dongle

`./codexion 2 800 200 100 100 5 50 fifo`. Coders 1 and 2, dongles 0 and 1. Coder 1 (odd) takes 0 then 1. Coder 2 (even) takes `right` first — coder 2's `right` is `(1+1)%2 = 0` — so it takes 0 then 1. **Both want dongle 0 first.**

```
t=0.0  MAIN: creates coder 1 thread, coder 2 thread, monitor thread

t=0.0  C1: coder_routine, id odd, no delay
       C1: take_two -> first = dongle 0
       C1: take_dongle(d0)
       C1: LOCK d0.lock                      <- C1 owns d0's lock
       C1: build_request -> LOCK state, seq 0->1, req={id:1,seq:1,dl:800}
                            UNLOCK state
       C1: heap_push -> d0.queue = [ {id:1,seq:1} ]
       C1: wait_for_turn -> dongle_ready?
                            taken=0 ok, now>=free_at(0) ok,
                            root.id=1 == 1 -> READY
       C1: heap_pop -> d0.queue = []
       C1: d0.taken = 1
       C1: broadcast d0.cond
       C1: UNLOCK d0.lock
       C1: log "0 1 has taken a dongle"

t=0.1  C2: id even -> precise_sleep 1 ms      <- the anti-thundering-herd stagger
       C2: take_two -> first = dongle 0 (right)
       C2: take_dongle(d0)
       C2: LOCK d0.lock
       C2: heap_push -> d0.queue = [ {id:2,seq:2} ]
       C2: wait_for_turn -> dongle_ready?
                            d0.taken == 1 -> NOT READY
       C2: sim_running? yes
       C2: cond_timedwait(d0.cond, d0.lock, now+1ms)
           ^^^ ATOMICALLY: unlocks d0.lock, sleeps
           ^^^ C2 is now off the CPU entirely

t=0.2  C1: take_dongle(d1)
       C1: LOCK d1.lock, push, ready immediately, taken=1, UNLOCK
       C1: log "0 1 has taken a dongle"
       C1: do_compile -> LOCK state, last_compile=0, compiles=1, UNLOCK
       C1: log "0 1 is compiling"
       C1: precise_sleep(200)

t=1.2  C2: timedwait times out, RE-LOCKS d0.lock automatically
       C2: loop re-checks dongle_ready -> d0.taken still 1 -> sleeps again
       (this repeats about 200 times, once per ms, costing almost nothing)

t=200  C1: drop_two
       C1: drop_dongle(d0): LOCK d0.lock
                            d0.taken = 0
                            d0.free_at = 200 + 50 = 250
                            broadcast d0.cond      <- wakes C2
                            UNLOCK d0.lock
       C1: drop_dongle(d1): same, d1.free_at = 250
       C1: log "200 1 is debugging", precise_sleep(100)

t=200  C2: woken by the broadcast, re-locks d0.lock
       C2: dongle_ready? taken=0 ok
                         now_ms()=200 < free_at=250 -> NOT READY
                         ^^^ THE COOLDOWN GATE
       C2: sleeps again

t=250  C2: wakes on the 1 ms tick
       C2: dongle_ready? taken=0 ok, 250 >= 250 ok, root.id=2 -> READY
       C2: heap_pop, d0.taken=1, broadcast, UNLOCK
       C2: log "250 2 has taken a dongle"
       C2: take_dongle(d1) -> d1.free_at=250, free -> granted
       C2: log "250 2 has taken a dongle"
       C2: log "250 2 is compiling"

t=300  C1: log "300 1 is refactoring", precise_sleep(100)
t=400  C1: take_two -> d0 is taken by C2 -> C1 queues and waits
       (roles now reversed; the pattern alternates for the rest of the run)
```

Everything the subject demands is visible in this trace: mutual exclusion at t=0.1, cooldown at t=200, no-barging via the root check, and the coder waiting asleep rather than spinning.

---

### `drop_dongle()`

**Purpose.** Release a dongle and start its cooldown.

```c
	pthread_mutex_lock(&dongle->lock);
	dongle->taken = 0;
	dongle->free_at = now_ms() + sim->cooldown;
	pthread_cond_broadcast(&dongle->cond);
	pthread_mutex_unlock(&dongle->lock);
```

`taken = 0` frees it. `free_at = now_ms() + sim->cooldown` stamps the earliest instant it may be granted again — computed **at release time**, which is exactly what "after being released, a dongle is unavailable until its cooldown has passed" means.

With `cooldown == 0`, `free_at` becomes `now_ms()`, and `dongle_ready`'s test `now_ms() < free_at` is immediately false. Zero cooldown needs no special case.

`pthread_cond_broadcast` **while still holding the lock.** Broadcasting after unlocking is legal but creates a window where a waiter could wake, check, and sleep again just before the broadcast arrives — costing it a full tick. Broadcasting inside guarantees waiters re-check after the state change.

**Why there is no "who owns this?" check.** Only the coder holding the dongle calls `drop_dongle` on it, because `drop_two` is called only after `take_two` succeeded. Adding an owner field would be defensive programming against a bug that the control flow makes impossible.

---

## 14. `src/dongle_pair.c`

### `take_two()`

**Purpose.** Acquire both dongles in the order that makes deadlock impossible.

**Returns.** 1 if both acquired, 0 if the simulation ended partway.

```c
	first = &coder->sim->dongles[coder->left];
	second = &coder->sim->dongles[coder->right];
	if (coder->id % 2 == 0)
	{
		first = &coder->sim->dongles[coder->right];
		second = &coder->sim->dongles[coder->left];
	}
	if (!take_dongle(coder, first))
		return (0);
	if (!take_dongle(coder, second))
	{
		drop_dongle(coder->sim, first);
		return (0);
	}
	return (1);
```

The default is left-then-right; even coders swap. `coder->id % 2 == 0` is the parity test — the remainder of division by 2 is 0 for even numbers.

**The failure path is the subtle part:**

```c
	if (!take_dongle(coder, second))
	{
		drop_dongle(coder->sim, first);
		return (0);
	}
```

If the second acquisition fails, this coder is **holding the first dongle**. Returning without releasing it would abandon a held dongle. During shutdown that is survivable — everyone is leaving anyway — but it is still wrong, and if the shutdown logic ever changed it would become a hang. Releasing before returning means no dongle is ever abandoned while held.

**Test yourself.** *Coder 3 (odd, left=2, right=3) holds dongle 2 and is waiting for dongle 3, which coder 4 holds. Coder 4 (even, left=3, right=0) took dongle 0 first. Is this a deadlock?*

> No. Coder 4 took dongle 0 first (its `right`), then wants dongle 3 (its `left`). But the premise says coder 4 *holds* dongle 3 — that means it already acquired both 0 and 3 and is compiling. It will finish and release both. Coder 3 then proceeds. For a deadlock you would need coder 4 to be waiting on something coder 3 holds, and coder 3 holds only dongle 2, which coder 4 never requests.

### `drop_two()`

```c
	drop_dongle(coder->sim, &coder->sim->dongles[coder->left]);
	drop_dongle(coder->sim, &coder->sim->dongles[coder->right]);
```

Release order does not matter. Releasing never blocks, so there is no ordering hazard — the asymmetry that matters on acquisition is irrelevant here. Note it uses `left` and `right` directly rather than the possibly-swapped `first`/`second`, because both get released regardless.

---

## 15. Edge cases across the arbitration layer

| # | Case | Behaviour |
|---|---|---|
| 1 | Normal: dongle free, cooldown passed, you are at the root | Granted immediately, no sleep |
| 2 | One coder total | `take_two` never runs; `lone_coder` takes dongle 0 and waits to burn out (Part 4) |
| 3 | Two coders want the same dongle | Both queue; the scheduler decides; the loser sleeps on the condvar |
| 4 | No dongle available | Coder sleeps, re-checking every 1 ms, costing no CPU |
| 5 | Dongle free but you are not at the root | Refused by gate 3 — fairness beats speed |
| 6 | Dongle free, root is you, cooldown pending | Refused by gate 2 until `free_at` |
| 7 | Simulation ends while you wait | `wait_for_turn` returns 0, request removed, `take_two` returns 0, coder exits the loop |
| 8 | Simulation ends while you hold the first dongle | `drop_dongle(first)` runs before returning; nothing is abandoned |
| 9 | `cooldown == 0` | `free_at == now_ms()`, gate 2 passes immediately, no special case needed |
| 10 | `heap_push` fails (impossible) | Lock released, returns 0, treated as a normal failure |

---

## Quick self-test on Part 3

**Q1.** Why is `wait_for_turn` a `while` loop and not an `if`?

> Three reasons, all real: POSIX allows spurious wakeups with no signal at all; another thread may have taken the dongle between the broadcast and this thread running; and `timedwait` returns on timeout with nothing changed. In every case the condition must be re-checked.

**Q2.** Why `broadcast` instead of `signal`?

> Waiters are not interchangeable — only the coder at the heap root can proceed. `signal` wakes one arbitrary waiter, which may be the wrong one; it would check, fail, and sleep again while the right coder stayed asleep. Broadcast wakes all, each re-checks, exactly one proceeds.

**Q3.** The dongle is free, its cooldown has expired, and coder 5 is awake wanting it. But the queue root is coder 2. What happens, and why is refusing correct?

> Coder 5 is refused by gate 3 and sleeps again. Refusing is correct because the subject makes arbitration mandatory: the dongle must be granted according to the scheduler, not to whichever thread the OS happened to wake first. Allowing coder 5 to take it would make FIFO and EDF meaningless.

**Q4.** Why does `take_dongle` call `log_state` after unlocking the dongle?

> To keep the lock hierarchy at two orders. Logging inside would create `dongle → print → state`, a third nesting order and a future deadlock risk. It also shortens the critical section, since `printf` is slow.

**Q5.** What exactly goes wrong if `sim->seq++` in `build_request` loses its mutex?

> Multiple coder threads write it, so increments are lost and two requests can share a `seq`. `heap_less` then returns 0 in both directions for that pair, so their relative order depends on incidental array positions. FIFO arbitration silently stops being FIFO, with no crash and no visible symptom.

**Q6.** Which Coffman condition does this project break, and how?

> Circular wait. Odd coders request left-then-right, even coders right-then-left, so two neighbours compete for the same *first* dongle. One of them blocks while holding nothing, and a thread holding nothing cannot be part of a wait-for cycle. The other three conditions are all kept, deliberately.

---
---

# PART 4 — Coders, the monitor, logging and timing

---

## 16. Concepts before this part

### 16.1 How time is measured

**Simple version.** The program needs to know "what time is it now" and "how long since X happened". It asks the operating system for the wall clock.

**Technical version.** `gettimeofday(&tv, NULL)` fills a `struct timeval`:

```c
struct timeval {
	time_t      tv_sec;   // whole seconds since 1 January 1970 (the epoch)
	suseconds_t tv_usec;  // microseconds within the current second, 0..999999
};
```

The value is **absolute**: seconds since 1970, roughly 1.78 billion today. Not the time since the program started.

There is a second clock type, `struct timespec`, used by `pthread_cond_timedwait`, which stores nanoseconds instead of microseconds:

```c
struct timespec {
	time_t tv_sec;    // seconds since the epoch
	long   tv_nsec;   // nanoseconds within the second, 0..999999999
};
```

Both are allowed by the subject. The project uses `gettimeofday` for measuring and converts to `timespec` only where the pthread API demands it.

### 16.2 Absolute versus relative time

This is a design decision worth understanding because it affects three files.

Your code stores **every** internal timestamp as absolute milliseconds since the epoch: `sim->start`, `coder->last_compile`, `dongle->free_at`, `req.deadline`. Only at the moment of printing does it subtract `sim->start` to produce the small relative number the subject wants.

**Why absolute internally?** Because `pthread_cond_timedwait` requires an **absolute** deadline, not a duration. If you stored relative times you would have to convert at every wait, adding `sim->start` back in — more arithmetic and more chances to get it wrong. Comparisons like `now_ms() < dongle->free_at` also work directly with no conversion.

**Why relative when printing?** The subject's example output starts at 0 and counts up in milliseconds. Printing 1757834521847 would be unreadable and would not match the expected format.

### 16.3 `usleep` and why sleeping is imprecise

`usleep(n)` suspends the calling thread for **at least** `n` microseconds. The guarantee is one-directional: never less, possibly more.

The OS scheduler decides when a sleeping thread runs again. If the machine is busy, a thread asking for 200 microseconds might get 400 or 1000. You cannot control this, only design around it.

**The naive approach and why it fails:**

```c
usleep(sim->compile_ms * 1000);    // WRONG for this project
```

Two separate problems:

1. **Overshoot.** Asking for 200 ms might deliver 210 ms. The coder compiles for 210 ms, its next compile starts 10 ms late, and with tight deadlines that drift accumulates into a burnout that should not have happened.
2. **Unresponsiveness.** A coder sleeping 200 ms in one call cannot notice that the simulation ended. The monitor stops the run at `t = 50` but this thread does not return until `t = 200`, so `pthread_join` blocks and the program takes longer to exit than it should.

Both are fixed by the same technique: sleep in small increments and re-check.

---

## 17. `src/time_utils.c`

### Purpose of the file

Three primitives everything else depends on: read the clock, convert to the pthread time format, and sleep accurately while staying responsive to shutdown.

---

### `now_ms()`

**Purpose.** Current wall-clock time in milliseconds since the epoch.

**Returns.** `long long` — milliseconds.

```c
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((long long)tv.tv_sec * 1000 + tv.tv_usec / 1000);
```

`tv` is a local, so it lives on the **calling thread's stack**. Every thread calling `now_ms` gets its own `tv`. This is why the function needs no mutex despite being called from six threads simultaneously — there is no shared state. Worth having ready as an answer.

`gettimeofday(&tv, NULL)` fills it. The second argument is an obsolete timezone parameter; `NULL` is the correct modern value.

**The arithmetic, and why the cast matters.**

- `tv.tv_sec * 1000` converts seconds to milliseconds.
- `tv.tv_usec / 1000` converts microseconds to milliseconds, discarding the sub-millisecond remainder by integer division.

`(long long)tv.tv_sec` casts **before** multiplying. If `tv_sec` were a 32-bit type, `tv_sec * 1000` would be computed in 32 bits and overflow catastrophically — 1.78 billion × 1000 is about 1.78 trillion, far beyond the roughly 2.1 billion a 32-bit signed integer holds. Casting first forces the whole expression into 64-bit arithmetic.

**Why the return type is `long long`.** An `int` cannot hold 1.78 trillion. Every timestamp field in the project is `long long` for the same reason.

**Precision note.** Truncating microseconds means `now_ms()` can be up to 0.999 ms behind the true time. This is why your measured burnout drift is consistently 1 ms rather than 0 — well inside the subject's 10 ms allowance, and the reason it never varies.

---

### `ms_to_timespec()`

**Purpose.** Convert absolute milliseconds into the `struct timespec` that `pthread_cond_timedwait` requires.

```c
	ts->tv_sec = ms / 1000;
	ts->tv_nsec = (ms % 1000) * 1000000;
```

Split the millisecond value into whole seconds and the leftover.

Worked example with `ms = 1757834521847`:

- `ts->tv_sec = 1757834521847 / 1000 = 1757834521` seconds
- `ms % 1000 = 847` milliseconds left over
- `ts->tv_nsec = 847 * 1000000 = 847000000` nanoseconds

**Why `* 1000000`?** One millisecond is a million nanoseconds. The remainder must be expressed in the field's unit.

**Why `tv_nsec` can never overflow its legal range.** `ms % 1000` is at most 999, so `tv_nsec` is at most 999,000,000 — under the one-billion limit the standard requires. Had the code written `ms * 1000000` without the modulo, `tv_nsec` would exceed a second and `pthread_cond_timedwait` would reject it with `EINVAL`, silently turning every wait into a busy spin.

**Why the result is written through a pointer rather than returned.** Returning a struct by value is legal but copies it; writing through a pointer lets the caller keep `ts` on its stack and reuse it every loop iteration. In `wait_for_turn` this function is called every millisecond per waiting thread, so avoiding a copy is worth it.

---

### `precise_sleep()`

**Purpose.** Sleep for `ms` milliseconds, accurately, while remaining able to abandon the sleep if the simulation ends.

```c
	end = now_ms() + ms;
	while (now_ms() < end)
	{
		if (!sim_running(sim))
			return ;
		usleep(200);
	}
```

**`end` is computed once, up front.** This is what makes the sleep accurate. The target instant is fixed at the start, so overshoot in any individual `usleep` does not accumulate. If one 200-microsecond nap takes 500 microseconds, the loop simply performs fewer remaining iterations — the wake-up time is still `end`.

Compare with the broken alternative of subtracting elapsed time each round: every rounding error would compound.

**`while (now_ms() < end)`** — re-read the clock each iteration and stop when the target is reached. Note this can overshoot by at most one iteration, roughly 200 microseconds, which is far below the 1 ms resolution of the output.

**`if (!sim_running(sim)) return ;`** — the responsiveness check, once per iteration, so at most ~200 microseconds after the simulation ends.

*What it prevents:* a coder sleeping through the end of the run. Say `time_to_refactor = 200` and a different coder burns out 5 ms into that refactor. Without this check the thread keeps sleeping for another 195 ms, and `main` cannot finish joining until it wakes. With it, the thread returns almost immediately.

*Concrete numbers:* a coder with `debug_ms = 200` that is 10 ms in when the run ends exits after roughly 200 microseconds instead of 190 ms.

**Why `usleep(200)` and not something larger or smaller?**

- Larger, say `usleep(5000)`: fewer wakeups but up to 5 ms of overshoot, which would show up in the logged timestamps as compile phases lasting 205 ms instead of 200.
- Smaller, say `usleep(10)`: more accurate in principle but thousands of syscalls per second per thread, and the OS will not honour such short sleeps precisely anyway.

200 microseconds keeps the error an order of magnitude below the 1 ms unit of the output. Your measured phase durations were exactly 200 ms with zero variance, which is this choice working.

**A subtle detail: `precise_sleep(coder->sim, 1)`** is called in `coder_routine` for the 1 ms even-coder stagger. With `ms = 1`, the loop runs a handful of 200-microsecond naps. It uses `precise_sleep` rather than a raw `usleep(1000)` so that even this tiny delay respects the shutdown flag.

**Edge case: `ms == 0`.** `end == now_ms()`, so `now_ms() < end` is false immediately and the function returns without sleeping. `./codexion 6 800 0 0 0 20 0 fifo` works for exactly this reason — one of the stress cases that passed.

---

## 18. `src/log.c`

### Purpose of the file

All output goes through here. Two functions, because there are two different rules: ordinary state messages must be suppressed once the run ends, and the burnout message must not be.

The subject: "A displayed state message should not be mixed up with another message."

### 18.1 Why printing needs a mutex

**Simple version.** `printf` is not one instruction. Two threads printing at once can produce a single garbled line.

**Technical version.** `printf` writes into a buffer and flushes it. Without serialisation, two threads can interleave mid-write:

```
Coder 1 wants: "201 1 is compiling\n"
Coder 3 wants: "201 3 is debugging\n"

possible output without a mutex:

201 1 is 201 3 is debugging
compiling
```

That line matches no format the subject allows. The scale is explicit: "interleaved log line ... means 0 to the project."

---

### `log_state()`

**Purpose.** Print one state change, but only while the simulation is running.

```c
	pthread_mutex_lock(&coder->sim->print);
	pthread_mutex_lock(&coder->sim->state);
	if (coder->sim->running)
	{
		stamp = now_ms() - coder->sim->start;
		printf("%lld %d %s\n", stamp, coder->id, msg);
	}
	pthread_mutex_unlock(&coder->sim->state);
	pthread_mutex_unlock(&coder->sim->print);
```

**Two mutexes, in a fixed order.** `print` first, then `state`. This is the second of the two nesting orders in the lock hierarchy from Part 3. It is consistent everywhere in the program.

**Why both are needed.** They do different jobs:

- `print` serialises the output stream so lines cannot interleave.
- `state` protects the read of `sim->running`, which the monitor writes.

Reading `running` without `state` would be a data race on a variable another thread modifies — exactly what helgrind, DRD and ThreadSanitizer look for.

**`if (coder->sim->running)` — the suppression gate.**

*What is checked:* is the simulation still alive? *If false:* print nothing.

*What it prevents:* output after the end. Consider the burnout sequence with `time_to_burnout = 500`:

```
t=501  Monitor: detects coder 1 missed its deadline
t=501  Monitor: stop_sim()  -> running = 0
t=501  Monitor: log_burnout -> "501 1 burned out"
t=502  Coder 3: was mid-refactor, wakes, calls log_state("is compiling")
                -> running is 0 -> prints NOTHING
```

Without the gate, coder 3's line would appear **after** `burned out`. The scale checks this directly: "The 'burned out' line must be the LAST line."

**Why the timestamp is computed inside the `if`.** No reason to read the clock if nothing will be printed. More importantly, computing it inside means the timestamp is taken while both locks are held, so the printed time cannot drift from the moment of printing.

**`stamp = now_ms() - coder->sim->start;`** converts absolute to relative. `sim->start` is read without extra protection because it is written once in `start_threads` before any thread exists and never changes — read-only sharing, as established in Part 1.

**The format string.** `"%lld %d %s\n"` matches the subject exactly: `timestamp_in_ms X message`. `%lld` is required for `long long`; using `%d` there would print garbage on a 64-bit value.

**A window worth knowing about.** A thread can pass the `running` check and then be paused by the OS before `printf` completes. The monitor cannot interleave its burnout line, because it needs `print`, which this thread holds. So the ordering guarantee survives: the worst case is a state line printed a fraction of a millisecond before the burnout line, never after.

---

### `log_burnout()`

**Purpose.** Print the final message. Deliberately unconditional.

```c
	pthread_mutex_lock(&coder->sim->print);
	stamp = now_ms() - coder->sim->start;
	printf("%lld %d burned out\n", stamp, coder->id);
	pthread_mutex_unlock(&coder->sim->print);
```

**The difference from `log_state`: no `state` lock and no `running` check.**

This is intentional and is the single exception in the program. By the time this runs, `stop_sim` has already set `running = 0`. If this function checked the flag, the burnout message — the one message that must always appear — would be suppressed.

**Why it is safe to skip the `state` lock.** The function reads only `sim->start` (immutable after startup) and `coder->id` (immutable after init). It reads nothing that any thread writes.

**Why it still takes `print`.** Another coder might be mid-`printf`. Taking `print` guarantees this line is not spliced into that one. It also creates the ordering: any state line in flight completes first, then the burnout line, then nothing more, because `running` is already 0.

**Test yourself.** *Why not set `running = 0` after printing instead of before?*

> Because the gap between printing and clearing the flag is a window in which other coders would pass the `running` check and print lines after the burnout message. Clearing first closes the window entirely. The cost is a sub-millisecond delay in the burnout timestamp, which is invisible against the 10 ms allowance.

---

## 19. `src/coder.c`

### Purpose of the file

The life of one coder thread: the compile-debug-refactor cycle, the stop conditions, and the one-coder special case.

The subject describes the cycle: "The coders alternatively compile, debug, or refactor... When a coder finishes compiling, they put both dongles back on the table and start debugging. Once debugging is done, they start refactoring."

---

### `coder_routine()`

**Purpose.** The thread entry point. Everything a coder does.

**Inputs.** `void *arg` — required by `pthread_create`'s signature. Actually a `t_coder *`.

**Returns.** `void *` — always `NULL`; nothing reads it.

```c
	coder = (t_coder *)arg;
```

**Why the cast.** `pthread_create` accepts `void *(*)(void *)`, a completely generic signature, because it must work for any thread function. `void *` is a pointer to an unknown type — it cannot be dereferenced. Casting back to `t_coder *` restores the type information so `coder->id` becomes meaningful.

This is safe because we know what was passed: `start_threads` passed `&sim->coders[i]`. The cast recovers exactly what was sent.

```c
	if (coder->sim->nb_coders == 1)
		return (lone_coder(coder));
```

The one-coder branch, taken before anything else. Detailed below.

```c
	if (coder->id % 2 == 0)
		precise_sleep(coder->sim, 1);
```

**The stagger.** Even-numbered coders wait 1 ms before their first attempt.

*What it prevents:* a thundering herd at `t = 0`. All coders start simultaneously and all rush the dongles at once. Every request lands in the same millisecond, so under EDF every deadline is identical and the tie-break decides everything; under FIFO the `seq` order is whatever the OS scheduler happened to produce.

Delaying even coders by 1 ms lets odd coders — who do not share a *first* dongle with each other in most ring sizes — establish themselves cleanly. The result is the clean opening you see in the logs: `1, 3, 5` take dongles first, then `2, 4`.

*This is an optimisation, not a correctness requirement.* Deadlock freedom comes from the acquisition ordering, not from the delay. Be clear about that distinction if asked — claiming the delay prevents deadlock is a common and wrong answer.

```c
	while (sim_running(coder->sim) && !coder_done(coder))
```

**Two stop conditions**, checked before every cycle.

`sim_running` — has the simulation ended, either from a burnout or because everyone finished? `&&` short-circuits, so if the run is over, `coder_done` is never called and no mutex is taken unnecessarily.

`coder_done` — has **this** coder reached its required compile count? A coder that has done its share stops rather than continuing to consume dongles its neighbours still need. The monitor then notices everyone is finished and ends the run.

```c
		if (!take_two(coder))
			break ;
```

Acquisition failed, meaning the run ended while waiting. `break` exits the loop and the thread returns.

```c
		do_compile(coder);
		drop_two(coder);
```

Compile with both dongles held, then release both. Releasing immediately after compiling — before debugging — is what the subject requires: "When a coder finishes compiling, they put both dongles back on the table and start debugging." Holding them through debug and refactor would cut throughput by two thirds.

```c
		if (coder_done(coder))
			break ;
```

**Checked again, right after compiling.** The compile that just happened may have been the last one required. Breaking here means the coder does not print `is debugging` and `is refactoring` after its final compile.

*This is a design decision you should own.* The alternative — always finishing the cycle — would produce two extra lines per coder at the end and delay the program's exit by `debug_ms + refactor_ms`. Neither is wrong; the subject does not specify. Be ready to say you chose to stop at the moment the requirement is met.

```c
		log_state(coder, "is debugging");
		precise_sleep(coder->sim, coder->sim->debug_ms);
		log_state(coder, "is refactoring");
		precise_sleep(coder->sim, coder->sim->refactor_ms);
```

The two remaining phases. After refactoring the loop repeats and the coder immediately tries to acquire dongles again — matching the subject: "After completing the refactoring phase, the coder will immediately attempt to acquire dongles and start compiling again."

Note neither phase holds a dongle. During debug and refactor the coder's dongles are available to its neighbours. That is where all the parallelism in this program comes from.

---

### `do_compile()`

**Purpose.** Record that a compile started, announce it, then spend the compile time.

```c
	pthread_mutex_lock(&coder->sim->state);
	coder->last_compile = now_ms();
	coder->compiles++;
	pthread_mutex_unlock(&coder->sim->state);
	log_state(coder, "is compiling");
	precise_sleep(coder->sim, coder->sim->compile_ms);
```

**`last_compile = now_ms()` — the deadline reset.** This single assignment is what keeps a coder alive. The subject: a coder burns out "if a coder did not start compiling within `time_to_burnout` milliseconds since the beginning of their last compile". Updating it here, at the **start** of the compile, restarts that clock.

*What if it were updated at the end of the compile instead?* The coder would get `compile_ms` of free extra life on every cycle, and the burnout rule would no longer match the subject's definition. With `compile_ms = 200` and `burnout = 500`, a coder would effectively have 700 ms — and the case the scale requires to burn out, `5 500 200 200 200`, would not burn out.

**`compiles++` — the completion counter.** Read by `coder_done` and by the monitor's `all_done` and `check_burnout`.

**Why both are updated inside one critical section.** They must change together, atomically, from the monitor's point of view. Imagine them in separate lock/unlock pairs:

```
Coder 1: LOCK state; last_compile = 4000; UNLOCK state
Monitor:                        LOCK state
                                reads last_compile = 4000 (new)
                                reads compiles      = 2    (old, not yet incremented)
                                UNLOCK state
Coder 1: LOCK state; compiles = 3; UNLOCK state
```

The monitor saw a half-updated coder. Here the consequence is mild, but the principle matters and it is the kind of thing an evaluator probes: **fields that change together must be updated under one lock**.

**Why the mutex cannot be dropped.** Covered in Part 1, but to restate precisely: only this thread writes these fields, so this is not a lost-update race. It is a **writer versus concurrent reader** race. Without the mutex the monitor could read a torn 64-bit `last_compile`, or read a stale value cached in a register, and declare a healthy coder burned out. That is a data race, and all three of helgrind, DRD and ThreadSanitizer would report it.

**Why `log_state` is called after unlocking `state`.** If it were called while holding `state`, the order would be `state → print`, the reverse of `log_state`'s own `print → state`. That is a textbook lock-order inversion and a genuine deadlock:

```
Coder 1: holds state, wants print
Coder 3: holds print, wants state
Both wait forever.
```

The code avoids it by releasing `state` first. This is the clearest concrete example of why the lock hierarchy exists, and a good one to have ready.

---

### `coder_done()`

**Purpose.** Has this coder reached its required compile count?

```c
	pthread_mutex_lock(&coder->sim->state);
	done = (coder->compiles >= coder->sim->nb_compiles);
	pthread_mutex_unlock(&coder->sim->state);
	return (done);
```

The comparison happens **inside** the lock and the result is copied into a local `int`. The local is then returned after unlocking. Returning `coder->compiles >= ...` directly after the unlock would read the shared field outside the mutex — the exact mistake this pattern avoids.

`>=` rather than `==` is defensive: if the count ever overshot, `==` would miss it and the coder would loop forever.

`nb_compiles` is read-only after parsing, but it is read here inside the lock anyway because it is free to do so and keeps the pattern uniform.

---

### `lone_coder()`

**Purpose.** Handle `nb_coders == 1`.

```c
	take_dongle(coder, &coder->sim->dongles[0]);
	while (sim_running(coder->sim))
		usleep(300);
	return (NULL);
```

**Why a special case at all.** With one coder, `init_coders` sets `left = 0` and `right = (0+1) % 1 = 0`. Both are dongle 0 — **the same dongle**. Running the normal path would call `take_dongle` twice on it:

1. First call: queue, granted, `taken = 1`, log.
2. Second call: queue, `dongle_ready` sees `taken == 1`, never becomes true, so the coder waits until burnout.

That would actually produce correct output by accident, but it relies on subtle behaviour and leaves a request permanently queued. The explicit branch makes the intent obvious.

**The behaviour it produces** matches the scale exactly: "A single coder has a single dongle but needs two to compile. The coder can therefore never compile and MUST burn out, around timestamp 800."

```
0 1 has taken a dongle
801 1 burned out
```

`take_dongle` succeeds and logs. Then the coder waits. `last_compile` is still `sim->start`, so the monitor sees `now_ms() - start` grow past `burnout` and declares the burnout.

`usleep(300)` rather than `precise_sleep` because there is no duration to be accurate about — just wait to be told the run is over. The return value of `take_dongle` is ignored because there is nothing to do either way.

---

## 20. `src/monitor.c`

### Purpose of the file

The supervisor thread. The subject requires it: "A separate monitor thread must detect burnout precisely and stop the simulation. The burnout log must be printed within 10 ms of the actual burnout time."

**Why a separate thread rather than coders checking themselves?** A coder sleeping for 200 ms cannot notice it died 3 ms ago. It would report the burnout 197 ms late, ten times outside the allowance. Only an independent thread that does nothing but watch can meet a 10 ms bound.

---

### `monitor_routine()`

```c
	sim = (t_sim *)arg;
	while (sim_running(sim))
	{
		i = 0;
		while (i < sim->nb_coders)
		{
			if (check_burnout(sim, &sim->coders[i]))
				return (NULL);
			i++;
		}
		if (all_done(sim))
		{
			stop_sim(sim);
			return (NULL);
		}
		usleep(300);
	}
	return (NULL);
```

The outer loop runs until the run ends. The inner loop scans every coder each pass.

**`return (NULL)` on detecting a burnout** — the monitor's job is over the instant it finds one. `check_burnout` has already called `stop_sim` and printed, so returning ends the thread and lets `pthread_join` in `main` proceed.

**`usleep(300)` — the polling interval.** This is what sets your measured 1 ms accuracy.

*Why 300 microseconds?* The subject allows 10 ms. Polling at 300 µs means the worst-case detection lag is 300 µs, plus up to 1 ms from `now_ms()`'s millisecond truncation. Your measurements showed a consistent 1 ms drift, comfortably inside the limit.

*Why not poll faster, say every 10 µs?* It would burn CPU for no measurable gain, since the output resolution is 1 ms.

*Why not slower, say every 5 ms?* Worst-case lag would approach 6 ms. Still legal, but with no safety margin on a loaded machine during an evaluation.

**The order inside the loop matters.** Burnout is checked for **all** coders before `all_done`. If a coder has burned out in the same instant the last coder finished, the burnout wins. That is the right precedence: a burnout is a failure of the simulation and should be reported.

---

### `check_burnout()`

**Purpose.** Decide whether one coder has burned out; if so, stop everything and report.

**Returns.** 1 if this coder burned out, 0 otherwise.

```c
	pthread_mutex_lock(&sim->state);
	last = coder->last_compile;
	full = (coder->compiles >= sim->nb_compiles);
	pthread_mutex_unlock(&sim->state);
	if (full || now_ms() - last <= sim->burnout)
		return (0);
	stop_sim(sim);
	log_burnout(coder);
	return (1);
```

**Both shared fields are copied into locals inside one critical section**, then the lock is released and the decision made on the copies. This keeps the lock held for a few instructions rather than across `now_ms()` and the printing, and it guarantees the two values describe the same instant.

**`full` — the finished-coder exemption.**

*What is checked:* has this coder already completed all required compiles? *If true:* skip the burnout check entirely.

*Why this is necessary.* A coder that finishes its quota exits its loop and stops compiling. Its `last_compile` freezes. Without this exemption, the monitor would watch that frozen timestamp age past `burnout` and declare a burnout for a coder that successfully finished.

*Concrete scenario:* `./codexion 5 800 200 200 100 3 0 fifo`. Coder 1 finishes its third compile at `t = 1500` and exits. Coders 4 and 5 still need one more each and finish at `t = 2400`. Without the exemption, at `t = 2301` the monitor would compute `2301 - 1500 = 801 > 800` and print `2301 1 burned out` — turning a successful run into a failure.

*Is exempting them correct?* The subject is silent on it. The reasoning: burnout models a coder who needs to compile and cannot. A coder with nothing left to do is not in that situation. Be ready to state this as a deliberate interpretation.

**`now_ms() - last <= sim->burnout` — the actual test.**

*Read it as:* "time since the last compile started is still within the allowance". If true, the coder is fine, return 0.

The condition is written as the **healthy** case, so burnout is the fall-through. Equivalently: a burnout occurs when `now_ms() - last > sim->burnout`, strictly greater.

*Why strictly greater and not `>=`?* At exactly `last + burnout` the coder has used precisely its allowance, not exceeded it. Using `>=` would kill coders one millisecond early and could fail borderline benchmark cases.

*Worked example, `burnout = 800`, `last = 200`:*

| `now_ms()` | `now - last` | `<= 800`? | Verdict |
|---|---|---|---|
| 900 | 700 | yes | alive |
| 1000 | 800 | yes | alive, exactly at the limit |
| 1001 | 801 | no | **burned out** |

**`stop_sim(sim);` before `log_burnout(coder);`** — the ordering discussed under `log_state`. Stopping first sets `running = 0`, so every other coder's `log_state` is suppressed and no line can appear after the burnout message.

---

### `all_done()`

**Purpose.** Have all coders reached the required compile count?

```c
	full = 1;
	i = 0;
	pthread_mutex_lock(&sim->state);
	while (i < sim->nb_coders)
	{
		if (sim->coders[i].compiles < sim->nb_compiles)
			full = 0;
		i++;
	}
	pthread_mutex_unlock(&sim->state);
	return (full);
```

Start optimistic, clear the flag on the first coder that falls short.

**The whole scan happens inside one critical section.** This is the important property: it produces a **consistent snapshot**. If the lock were taken and released per coder, coder 1 could finish after you checked it and coder 5 could finish before you reached it, and you might conclude "not all done" on a pass where all were in fact done — or worse, report done based on values read at different instants.

**Why it does not `break` early.** Setting `full = 0` and continuing to scan costs a few extra comparisons. Breaking would need either a `break` inside the lock followed by an unlock on two paths, or restructuring. The Norm's constraints make the simple form preferable, and the cost is trivial.

This implements the subject's second stop condition: "If all coders have compiled at least this many times, the simulation stops."

---

### `sim_running()` and `stop_sim()`

```c
int	sim_running(t_sim *sim)
{
	pthread_mutex_lock(&sim->state);
	running = sim->running;
	pthread_mutex_unlock(&sim->state);
	return (running);
}
```

Read the flag under the lock, copy to a local, return the copy. Called from every thread, constantly — in `coder_routine`'s loop condition, in `precise_sleep`, in `wait_for_turn`, in the monitor's loop.

**Why the mutex is not optional here.** Without it the compiler may keep `sim->running` in a register across loop iterations, since nothing in the loop appears to modify it. The thread would then never observe the change and would spin forever. Locking forces a genuine memory read each call. This is a **visibility** problem as much as an atomicity one, and it is the answer to "surely reading a single `int` is atomic anyway?"

```c
void	stop_sim(t_sim *sim)
{
	pthread_mutex_lock(&sim->state);
	sim->running = 0;
	pthread_mutex_unlock(&sim->state);
	i = 0;
	while (i < sim->ready)
	{
		pthread_mutex_lock(&sim->dongles[i].lock);
		pthread_cond_broadcast(&sim->dongles[i].cond);
		pthread_mutex_unlock(&sim->dongles[i].lock);
		i++;
	}
}
```

**Two phases, and the separation is deliberate.**

Phase one clears the flag and **releases `state` before phase two**. This is what keeps the lock hierarchy intact. If `state` were held while acquiring dongle locks, the order would be `state → dongle`, the reverse of `build_request`'s `dongle → state`, and you would have a genuine deadlock:

```
Monitor:  holds state, wants dongles[2].lock
Coder 3:  holds dongles[2].lock, wants state (inside build_request)
Both wait forever. The program hangs with no output.
```

This is the single most important reason the function is written in two phases, and it is exactly the kind of thing helgrind's lock-order checker looks for.

Phase two wakes every waiter. Coders asleep in `wait_for_turn` re-check `sim_running`, see 0, cancel their requests and return.

**Why broadcast is needed at all**, given the 1 ms tick would catch it anyway: the tick is a safety net, not the mechanism. Broadcasting means threads exit within microseconds rather than up to a millisecond, and the code would still be correct if the tick were removed.

**`i < sim->ready`, not `nb_coders`** — `ready` counts fully initialised dongles. If `init_dongles` failed partway and `main` calls `stop_sim` on the error path, this avoids locking a mutex that was never initialised.

---

## 21. Full burnout simulation, five threads

`./codexion 5 500 200 200 200 10 0 fifo` — one of the scale's required cases. Cycle time is 200+200+200 = 600 ms against a 500 ms deadline, so a burnout is guaranteed.

```
t=0     MAIN: start = now_ms(); all coders last_compile = start
              creates 5 coder threads + monitor

t=0     C1,C3,C5 (odd): take left first -> d0, d2, d4
        C2,C4 (even): sleep 1 ms

t=0     C1: gets d0, then wants d1 (free) -> gets it
            log "0 1 has taken a dongle" x2
            do_compile: LOCK state, last_compile=0, compiles=1, UNLOCK
            log "0 1 is compiling"
t=0     C3: gets d2, then d3 -> compiles
            log "0 3 is compiling"
t=0     C5: gets d4, wants d0 -> taken by C1 -> queues, sleeps

t=1     C2: wakes, wants d2 (its right) -> taken by C3 -> queues, sleeps
t=1     C4: wakes, wants d4 (its right) -> taken by C5 -> queues, sleeps

t=0..500  MONITOR: every 300 us, for each coder:
            now - last_compile vs 500
            all under 500 -> nothing happens

t=200   C1: precise_sleep(200) ends
            drop_two -> d0.taken=0 free_at=200
                        d1.taken=0 free_at=200
                        broadcast both
            log "200 1 is debugging"
t=200   C5: woken by d0's broadcast, re-checks
            d0 free, cooldown 0, root is C5 -> GRANTED
            log "200 5 has taken a dongle"
            C5 already holds d4 -> compiles
            do_compile: last_compile=200, compiles=1
            log "200 5 is compiling"

t=200   C3: same, releases d2,d3, C2 acquires d2 then d1 -> compiles
            log "200 2 is compiling"

t=400   C1: log "400 1 is refactoring"
t=400   C5,C2: release, C4 finally gets its pair -> compiles at ~400

t=600   C1: refactor ends, loops, take_two -> d0 and d1
            but C4 or others may hold them -> C1 queues and waits
            C1's deadline was 0 + 500 = 500  <-- ALREADY PASSED

t=501   MONITOR: check_burnout(C1)
            LOCK state; last=0; full = (1 >= 10)=false; UNLOCK
            full is false
            now_ms() - 0 = 501 <= 500 ? NO
            -> stop_sim(sim)
                 LOCK state; running=0; UNLOCK state
                 for each of 5 dongles: LOCK, broadcast, UNLOCK
            -> log_burnout(C1)
                 LOCK print; printf "501 1 burned out"; UNLOCK print
            -> return 1
        MONITOR: returns NULL, thread ends

t=501   C2..C5: wherever they are:
          - in precise_sleep -> sim_running() is 0 -> return within ~300 us
          - in wait_for_turn -> woken by broadcast -> sim_running() is 0
                                -> heap_remove, return 0
                                -> take_two returns 0 -> break
          - about to log_state -> running is 0 -> prints nothing
        all coder threads return NULL

t=501   MAIN: pthread_join(monitor) returns
              pthread_join(coder 1..5) return
              destroy_sim: destroy 5 dongle mutexes + condvars,
                           free 5 heaps, free dongles, free coders,
                           destroy state and print
              return 0
```

The output ends with `501 1 burned out` as the final line, 1 ms after the true deadline of 500.

---

## Quick self-test on Part 4

**Q1.** Why is `precise_sleep` a loop of `usleep(200)` instead of one `usleep(ms * 1000)`?

> Two reasons. Accuracy: `end` is fixed once, so overshoot in any single nap does not accumulate. Responsiveness: `sim_running` is re-checked every ~200 µs, so a coder abandons a long sleep almost immediately when the run ends instead of blocking `pthread_join` for the remainder.

**Q2.** Why does `log_state` check `running` while `log_burnout` deliberately does not?

> `stop_sim` sets `running = 0` before the burnout is printed, so ordinary state lines are suppressed and nothing can appear after the final message. If `log_burnout` made the same check, the one message that must always print would be suppressed.

**Q3.** `do_compile` unlocks `state` before calling `log_state`. What happens if it does not?

> `log_state` locks `print` then `state`. Holding `state` while calling it produces the order `state → print`, the reverse of the hierarchy. One thread holding `state` wanting `print`, another holding `print` wanting `state`, and both wait forever. A real, reproducible deadlock.

**Q4.** Why does `check_burnout` skip coders that have finished their required compiles?

> A finished coder stops compiling, so its `last_compile` freezes and would eventually exceed the burnout window. Without the exemption the monitor would declare a burnout for a coder that succeeded, turning a passing run into a failure. Example: coder 1 finishes at t=1500 with `burnout = 800`, and at t=2301 would be wrongly killed while others are still working.

**Q5.** Why does `stop_sim` release `state` before locking the dongle mutexes?

> To preserve the lock hierarchy. `build_request` takes `dongle → state`. Holding `state` while taking a dongle lock would create the reverse order and a genuine deadlock between the monitor and any coder inside `build_request`.

**Q6.** `last_compile` is set at the **start** of the compile. What changes if it were set at the end?

> Every coder would gain `compile_ms` of extra life per cycle, contradicting the subject's definition of the deadline as running from "the beginning of their last compile". The scale's required burnout case `5 500 200 200 200` would stop burning out, failing the test.

---
---

# PART 5 — Cleanup, and the whole picture

---

## 22. Concepts before `cleanup.c`

### 22.1 What a memory leak actually is

**Simple version.** You asked the system for memory and never gave it back. The system cannot reuse it.

**Technical version.** Every `malloc` returns a block the allocator marks as in use. `free` marks it available again. If the program loses the last pointer to a block without freeing it, that block is unreachable and unreclaimable for the process's lifetime.

When the process exits the OS reclaims everything anyway, so a leak in a short program causes no practical harm. It still counts as a failure here: the subject says "All heap-allocated memory must be properly freed when necessary. Memory leaks will not be tolerated," and the scale has a dedicated Leaks flag that means 0.

**The three ways this program could leak:**

| Allocation | Count | Freed by |
|---|---|---|
| `sim->dongles` | 1 | `destroy_dongles` |
| `sim->coders` | 1 | `destroy_sim` |
| `dongles[i].queue.data` | `nb_coders` | `heap_free` per dongle |

With 5 coders that is 7 blocks. Your valgrind run reported 12 allocations and 12 frees — the extra 5 come from the pthread library allocating per-thread stacks, which it frees on join.

### 22.2 Destroying synchronisation objects

`pthread_mutex_init` and `pthread_cond_init` may allocate internal resources. `pthread_mutex_destroy` and `pthread_cond_destroy` release them.

Two rules govern when destruction is legal:

1. **No thread may be using the object.** Destroying a locked mutex, or one a thread is blocked on, is undefined behaviour. This is why `main` joins every thread before calling `destroy_sim`.
2. **The object must have been initialised.** Destroying uninitialised memory is undefined behaviour. This is the entire reason `sim->ready` exists.

On Linux these usually do nothing visible, so forgetting them rarely produces a valgrind error. They are still required — the subject says "The simulation also destroys the initialized mutexes and joins all created threads before cleanup," and a picky evaluator will read for them.

---

## 23. `src/cleanup.c`

### Purpose of the file

Undo everything `init.c` built, in the right order, correctly on **every** exit path — normal end, burnout, failed allocation, failed thread creation.

---

### `destroy_dongles()`

```c
	if (!sim->dongles)
		return ;
	i = 0;
	while (i < sim->ready)
	{
		pthread_mutex_destroy(&sim->dongles[i].lock);
		pthread_cond_destroy(&sim->dongles[i].cond);
		i++;
	}
	i = 0;
	while (i < sim->nb_coders)
	{
		heap_free(&sim->dongles[i].queue);
		i++;
	}
	free(sim->dongles);
	sim->dongles = NULL;
```

**`if (!sim->dongles) return ;`** — the array may not exist at all. If `malloc` failed in `init_dongles`, `sim->dongles` is `NULL` (from `main`'s `memset`), and `sim->dongles[0].lock` would dereference address 0 and segfault. This guard is what makes the out-of-memory path safe.

**Two separate loops with two different bounds.** This looks redundant and is not.

The first loop runs to `sim->ready` — the count of dongles whose mutex and condvar were actually initialised. Destroying an uninitialised mutex is undefined behaviour.

The second loop runs to `sim->nb_coders` — every slot in the array. It is safe to go further here because `heap_free` calls `free(heap->data)`, and for a dongle that never got `heap_init`, `data` is `NULL` from the `memset`. `free(NULL)` is defined by the standard to do nothing.

*Worked failure case:* 5 coders, `heap_init` fails on dongle 2 (the third).

```
init_dongles progress:
  i=0: heap ok, mutex init, cond init, ready = 1
  i=1: heap ok, mutex init, cond init, ready = 2
  i=2: heap_init FAILS -> return 0
       (dongle 2's mutex and cond were never created)

destroy_dongles:
  loop 1 (i < ready = 2): destroys mutex+cond for dongles 0 and 1 only.
                          Dongle 2's uninitialised mutex is not touched.
  loop 2 (i < nb_coders = 5): heap_free on all 5.
                          Dongles 0,1 free real arrays.
                          Dongles 2,3,4 have data == NULL -> free(NULL), no-op.
  free(sim->dongles)
```

Nothing leaks and nothing is destroyed that was never created. This is what `ready` buys you, and it is the sort of detail that separates a careful implementation from one that happens to work.

**`sim->dongles = NULL;` after freeing** — prevents a double free if the function ran twice, and turns a stale-pointer bug into an immediate, obvious crash rather than silent corruption of recycled memory.

---

### `destroy_sim()`

```c
	destroy_dongles(sim);
	free(sim->coders);
	sim->coders = NULL;
	pthread_mutex_destroy(&sim->state);
	pthread_mutex_destroy(&sim->print);
```

**Order: dongles, then coders, then the global mutexes.** Reverse of construction, which is the conventional discipline. Here it is not strictly required — the three are independent — but reversing construction order is the habit that keeps you safe when they are not.

**`free(sim->coders)` has no NULL guard** because `free(NULL)` is explicitly defined as a no-op. If `init_coders` never ran, `coders` is `NULL` from the `memset` and this line does nothing.

**The two global mutexes are destroyed unconditionally.** Safe because `init_sim` initialises them as its very first action, before anything can fail. Any path that reaches `destroy_sim` has passed that point — `main` only calls it after `init_sim` was attempted.

**Why `destroy_sim` is called on both success and failure.** `main` has exactly two call sites:

```c
	if (!init_sim(&sim))
	{
		destroy_sim(&sim);     // failure path
		return (1);
	}
	...
	join_threads(&sim, made, started);
	destroy_sim(&sim);         // success path
```

One function handling both is possible only because it tolerates partial construction. That tolerance is the whole design of this file.

---

## 24. All lifecycles, end to end

### 24.1 Program execution flow

```
1. SHELL       ./codexion 4 800 200 100 100 3 50 fifo
2. PARSE       parse_args -> parse_number x7, parse_times, parse_sched
                 failure -> usage_error, exit 1, nothing allocated
3. EARLY EXIT  nb_compiles == 0 -> exit 0, no output
4. INIT        init_sim -> mutex_init(state, print)
                        -> init_dongles -> malloc + heap_init + mutex/cond per dongle
                        -> init_coders  -> malloc + id/left/right/sim per coder
                 failure -> destroy_sim, exit 1
5. LAUNCH      start_threads -> start = now_ms()
                             -> every coder last_compile = start
                             -> pthread_create x nb_coders (coder_routine)
                             -> pthread_create x 1        (monitor_routine)
6. RUN         main blocks in join_threads
               coders cycle; monitor polls every 300 us
7. STOP        either check_burnout fires  -> stop_sim + log_burnout
                  or  all_done is true     -> stop_sim
8. DRAIN       running = 0, broadcast on every dongle
               coders exit their loops and return NULL
9. JOIN        pthread_join(monitor), then each coder
10. CLEANUP    destroy_sim -> destroy mutexes/condvars, free heaps, free arrays
11. EXIT       return 0 (or 1 if thread creation had failed)
```

### 24.2 Thread lifecycle

```
CREATED       pthread_create in start_threads, given &sim->coders[i]
RUNNING       coder_routine: cast arg, check nb_coders==1, stagger if even
LOOPING       while (sim_running && !coder_done):
                  take_two -> do_compile -> drop_two -> debug -> refactor
EXITING       loop condition false, or take_two returned 0 -> break
TERMINATED    return NULL
REAPED        pthread_join in main releases the thread's resources
```

The monitor's is shorter: created last, polls, returns `NULL` on the first burnout or when `all_done` is true, joined first.

### 24.3 Dongle lifecycle

```
                 init_dongles
                      |
                      v
              +---------------+
              |   AVAILABLE   |  taken=0, free_at=0, queue empty
              +---------------+
                      |
                      |  a coder calls take_dongle:
                      |    heap_push (now in the queue)
                      v
              +---------------+
              |   REQUESTED   |  taken=0, queue non-empty
              +---------------+
                      |
                      |  dongle_ready returns 1 for the root coder:
                      |    !taken  AND  now >= free_at  AND  root is me
                      |    heap_pop, taken = 1
                      v
              +---------------+
              |     HELD      |  taken=1, exclusive to one coder
              +---------------+
                      |
                      |  compiling happens here (both dongles held)
                      |  drop_dongle:
                      |    taken = 0, free_at = now + cooldown, broadcast
                      v
              +---------------+
              |  COOLING DOWN |  taken=0, now < free_at
              +---------------+
                      |
                      |  time passes until now >= free_at
                      v
                 back to AVAILABLE
                      |
                      |  destroy_dongles
                      v
                  DESTROYED
```

### 24.4 Compile lifecycle

```
take_two(first)   -> log "has taken a dongle"
take_two(second)  -> log "has taken a dongle"
do_compile        -> last_compile = now_ms()   <- the deadline resets HERE
                     compiles++
                  -> log "is compiling"
                  -> precise_sleep(compile_ms)
drop_two          -> both dongles released, cooldowns start
                  -> log "is debugging"
                  -> precise_sleep(debug_ms)
                  -> log "is refactoring"
                  -> precise_sleep(refactor_ms)
                  -> immediately back to take_two
```

### 24.5 Monitoring lifecycle

```
every 300 microseconds:
    for each coder:
        snapshot last_compile and compiles under state
        if the coder is finished        -> skip it
        if now - last_compile <= burnout -> healthy, next coder
        otherwise:
            stop_sim   (running = 0, broadcast every dongle)
            log_burnout
            end the monitor thread
    if every coder reached nb_compiles:
        stop_sim
        end the monitor thread
```

---

## 25. Reference tables

### 25.1 Every mutex

| Mutex | Count | Protects | Locked in | Why it exists |
|---|---|---|---|---|
| `dongle[i].lock` | `nb_coders` | `taken`, `free_at`, `queue` of that dongle | `take_dongle`, `drop_dongle`, `stop_sim` | Prevents dongle duplication and heap corruption |
| `sim->state` | 1 | `running`, `seq`, every coder's `compiles` and `last_compile` | `build_request`, `do_compile`, `coder_done`, `sim_running`, `stop_sim`, `check_burnout`, `all_done`, `log_state` | Safe coder-to-monitor communication and the stop flag |
| `sim->print` | 1 | `stdout` | `log_state`, `log_burnout` | Prevents interleaved output lines |

**Lock hierarchy — only two nesting orders exist anywhere:**

```
dongle->lock  ->  sim->state       (build_request; sim_running inside wait_for_turn)
sim->print    ->  sim->state       (log_state)
```

The reverse orders never occur. No dongle lock is taken while holding `state`; `print` is never taken while holding `state`. That is why the mutexes themselves cannot deadlock.

### 25.2 Every shared variable

| Variable | Type | Meaning | Written by | Read by | Protected by |
|---|---|---|---|---|---|
| `sim->running` | `int` | 1 while the simulation is alive | monitor (`stop_sim`), main on failure | all threads | `state` |
| `sim->seq` | `long long` | Global arrival counter for FIFO | every coder (`build_request`) | same | `state` |
| `coder->compiles` | `int` | Completed compiles for this coder | that coder only | monitor, that coder | `state` |
| `coder->last_compile` | `long long` | Start of the most recent compile | that coder only | monitor, that coder | `state` |
| `dongle->taken` | `int` | 1 while held | whoever takes or drops it | any coder checking readiness | that dongle's `lock` |
| `dongle->free_at` | `long long` | Earliest instant it may be granted | releasing coder | any coder checking readiness | that dongle's `lock` |
| `dongle->queue` | `t_heap` | Pending requests | any coder pushing, popping, removing | same | that dongle's `lock` |
| `sim->start`, `burnout`, `compile_ms`, `debug_ms`, `refactor_ms`, `cooldown`, `nb_coders`, `nb_compiles`, `mode` | mixed | Configuration | before threads exist | all threads | **none needed** — read-only after startup |
| `coder->id`, `left`, `right`, `sim` | mixed | Identity and wiring | `init_coders`, before threads | that coder | **none needed** — read-only |

That last pair of rows is the point most people get wrong. A data race requires at least one writer concurrent with another access. Configuration values are written once before `pthread_create` and never again, so reading them unlocked from six threads is correct.

### 25.3 Every race condition and how it is prevented

| # | The race | Without protection | Prevented by |
|---|---|---|---|
| 1 | Two coders granted the same dongle | Both compile with one dongle; dongle duplication | `dongle->lock` held across the `dongle_ready` check, `heap_pop` and `taken = 1` |
| 2 | `sim->seq++` from several coders | Lost increments, duplicate `seq`, FIFO order becomes undefined | `state` in `build_request` |
| 3 | Coder writes `last_compile` while the monitor reads it | Torn or stale read; a healthy coder declared burned out | `state` in `do_compile` and `check_burnout` |
| 4 | Coder writes `compiles` while the monitor reads it | Monitor sees a half-updated coder; wrong stop decision | Same critical section as #3 |
| 5 | Monitor writes `running` while coders read it | Compiler caches it in a register; coders loop forever; program hangs | `state` in `stop_sim` and `sim_running` |
| 6 | Two threads `printf` at once | Two messages spliced into one line — an automatic 0 | `print` in `log_state` and `log_burnout` |
| 7 | Heap rearranged while another thread reads the root | `heap_peek` returns a pointer into a shifting array; wrong coder granted | That dongle's `lock` around every heap operation |
| 8 | `all_done` reading coders one at a time | Inconsistent snapshot; wrong stop decision | Whole scan inside one `state` critical section |

### 25.4 Deadlock analysis

**Resource deadlock (dongles).** Broken via circular wait: odd coders take left-then-right, even coders right-then-left. Two neighbours compete for the same *first* dongle, so one blocks while holding nothing, and a thread holding nothing cannot close a cycle.

**Mutex deadlock.** Prevented by the two-order lock hierarchy above. The most important instance is `stop_sim`, which releases `state` before taking any dongle lock — holding it would create `state → dongle`, the reverse of `build_request`, and hang the monitor against any coder building a request.

**Self-deadlock.** Mutexes are non-recursive, so locking one twice in a thread would hang it permanently. Avoided because `dongle_ready` does no locking of its own and is only called with the lock already held.

**Lost wakeup.** Prevented by `pthread_cond_timedwait` atomically unlocking and sleeping, plus the `while` loop that re-checks, plus the 1 ms tick as a safety net.

**Verified by tooling:** DRD reports 0 errors, ThreadSanitizer 0 warnings, and helgrind only the glibc timeout artifact reproduced with provably correct code.

### 25.5 Memory ledger

| Block | Allocated in | Size | Freed in |
|---|---|---|---|
| `sim->dongles` | `init_dongles` | `sizeof(t_dongle) * nb_coders` | `destroy_dongles` |
| `sim->coders` | `init_coders` | `sizeof(t_coder) * nb_coders` | `destroy_sim` |
| `queue.data` × `nb_coders` | `heap_init` | `sizeof(t_request) * nb_coders` each | `heap_free` via `destroy_dongles` |

Nothing is allocated after `init_sim` returns. No `malloc` runs while threads are active — requests are stack values copied into the pre-allocated heap arrays. That single design choice removes all allocation-failure handling from the concurrent part of the program.

---

## 26. Edge cases handled

| # | Case | Handling |
|---|---|---|
| 1 | Wrong argument count | `ac != 9` → usage, exit 1 |
| 2 | Negative, decimal, signed, or non-numeric values | `parse_number` rejects any non-digit character |
| 3 | Value above `INT_MAX` | Overflow checked per digit |
| 4 | Empty string argument | `if (!str[0])` rejects it |
| 5 | `time_to_burnout == 0` | Rejected — a coder would die before it could act |
| 6 | `nb_coders` 0 or above 200 | Rejected |
| 7 | Scheduler not exactly `fifo`/`edf` | `parse_sched` rejects, case-sensitively |
| 8 | `nb_compiles == 0` | Clean exit 0, no output, no threads |
| 9 | One coder | `lone_coder`: takes one dongle, burns out on schedule |
| 10 | `compile_ms`/`debug_ms`/`refactor_ms` all 0 | `precise_sleep(0)` returns immediately |
| 11 | `cooldown == 0` | `free_at = now_ms()`, gate passes immediately, no special case |
| 12 | `malloc` fails at any point | Partial cleanup via `ready` and NULL checks, exit 1, no leak |
| 13 | `pthread_create` fails partway | `stop_sim`, join only the threads created, cleanup, exit 1 |
| 14 | Run ends while a coder waits for a dongle | Broadcast wakes it, request removed, clean exit |
| 15 | Run ends while a coder holds one of two dongles | `take_two` releases the first before returning |
| 16 | Run ends mid-sleep | `precise_sleep` checks `running` every ~200 µs |
| 17 | A coder finishes its quota while others work | Exempted from burnout checks; exits and frees its dongles |
| 18 | Equal EDF deadlines | Tie-break on lower ID, fully deterministic |
| 19 | Output after the burnout line | `log_state` suppressed by the `running` gate |
| 20 | Infeasible parameters | Burnout reported honestly rather than hidden |

---

## 27. What is still weak — be honest about these

These are the things I would raise before an evaluator finds them. Knowing your own limitations is worth more at defense than pretending there are none.

**1. Throughput falls short of optimal at large cooldowns.** Measured: `5 3000 200 200 200 10 800` burns out under both FIFO and EDF. The cause is a combination of hold-and-wait — a coder holds its first dongle while queueing for the second, leaving it idle — and the no-barging rule, where a dongle sits free because its queue root is not yet able to use it. At cooldown 400 there is enough slack that it never matters; at 600 and above it does. Note the aggregate throughput measured at 100% of the theoretical ceiling, so the failure is in *distribution* across coders, not total work done. The parameters sit at the feasibility boundary: the optimal hand-computed schedule gets the last coder its second compile at exactly t=3000 against a 3000 ms deadline.

**2. A fix exists but conflicts with the subject.** Allowing a waiting coder to take a free dongle out of queue order when it could immediately use both would raise throughput. The subject forbids it: "when multiple coders request the same dongle, the dongle must grant access according to `scheduler`." The current behaviour is the compliant one. Say this explicitly if challenged — it is a requirement, not an oversight.

**3. Lines from different coders interleave between the second "has taken a dongle" and "is compiling".** Nothing is wrong on a single line, but a strict reading of the scale's "immediately preceded by exactly two 'has taken a dongle' lines" could be questioned. Per coder the invariant always holds. Making them strictly adjacent would require holding `print` across two log calls, which lengthens a critical section for a cosmetic gain.

**4. The 200-coder cap is my choice, not the subject's.** The subject sets no maximum. 200 matches the scale's testing guidance. If an evaluator wants 500, change `MAX_CODERS`.

**5. Stopping the cycle immediately after the final compile is an interpretation.** No `is debugging` or `is refactoring` line appears after a coder's last required compile. The subject does not specify. The alternative adds two lines per coder and delays exit.

**6. Exempting finished coders from burnout checks is also an interpretation.** Without it a coder that succeeded would be declared burned out while slower coders finish. Defensible, but it is a decision, not a rule from the subject.

**7. `pthread_mutex_init` return values are not checked.** They can theoretically fail under resource exhaustion. In practice they do not on Linux, and checking would require restructuring to stay inside the Norm's 25-line limit. A real gap, if a small one.

**8. Timing depends on the machine.** All phases measured exactly 200 ms here, but a heavily loaded evaluation machine may show drift. The scale allows for this: "On some slow hardware, precise timing may vary slightly; discuss borderline cases honestly."

---

## 28. Final self-test

**Q1.** Why does `destroy_dongles` use `ready` for one loop and `nb_coders` for the other?

> The first destroys mutexes and condvars, which is undefined behaviour on uninitialised objects, so it must stop at the count that were actually initialised. The second calls `heap_free`, which calls `free(data)`, and `data` is `NULL` for any dongle that never got `heap_init` — and `free(NULL)` is a defined no-op. So the second loop can safely cover every slot.

**Q2.** Name every variable that is shared between threads but needs no mutex, and justify it.

> `sim->start`, `burnout`, `compile_ms`, `debug_ms`, `refactor_ms`, `cooldown`, `nb_coders`, `nb_compiles`, `mode`, and each coder's `id`, `left`, `right`, `sim`. All are written before `pthread_create` and never modified afterwards. A data race requires a concurrent writer; read-only sharing has none.

**Q3.** Walk through what happens between the monitor detecting a burnout and `main` returning.

> `check_burnout` calls `stop_sim`, which sets `running = 0` under `state`, releases it, then locks each dongle in turn to broadcast. `log_burnout` takes `print` and writes the final line. The monitor returns `NULL`. Coders waiting on a condvar wake, see `running == 0`, call `heap_remove`, and return 0 from `take_dongle`; `take_two` releases any dongle already held and returns 0; the coder breaks out of its loop. Coders in `precise_sleep` return within ~200 µs. Coders about to log print nothing. All return `NULL`. `main`'s `pthread_join` calls return, `destroy_sim` destroys 2 + 2×`nb_coders` synchronisation objects and frees `nb_coders` + 2 blocks, and `main` returns 0.

**Q4.** Which single line would you change to turn FIFO into LIFO, and why is that sufficient?

> `return (a->seq < b->seq);` becomes `>` in `heap_less`. It is sufficient because the heap keeps whichever request wins that comparison at its root, and `dongle_ready` grants the dongle only to the coder at the root. All policy lives in that one comparison; nothing else in the program knows which scheduler is running.

**Q5.** Why are there no `malloc` calls once threads are running?

> Every request is a stack value copied into a pre-allocated heap array sized at `nb_coders`, which is provably enough because each coder can hold at most one outstanding request per dongle. This removes allocation failure from the concurrent code entirely and means no per-request memory can leak.

---

That completes all five parts: every file, every function, every line.

The three things evaluators press hardest on are `wait_for_turn`'s three gates, `heap_less` as the single home of scheduling policy, and the lock hierarchy with `stop_sim` as its clearest illustration. If you can explain those three from memory, the rest follows.
