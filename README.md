*This project has been created as part of the 42 curriculum by feel-idr*

# Codexion

## Description

Codexion simulates coders competing for shared USB dongles to compile quantum code.
Each coder repeatedly compiles (requires two dongles), debugs, and refactors.
The simulation stops when a coder burns out or all coders finish their required compilations.

---

## Instructions

```bash
make
./codexion <nb_coders> <burnout_ms> <compile_ms> <debug_ms> <refactor_ms> <nb_compiles> <cooldown_ms> <fifo|edf>
```

Example:
```bash
./codexion 4 800 200 100 100 3 50 fifo
```

---

## Scheduler

### FIFO

Coders waiting for a dongle are served according to the order in which their requests were registered.

### EDF

Coders are prioritized according to their burnout deadline. If two coders have the same deadline, the coder with the lower ID has priority.

---

## Blocking cases handled

- **Deadlock prevention**: even coders take right dongle first, odd coders take left first + 1ms delay at startup.
- **Starvation prevention**: Scheduling: FIFO grants access according to request order, while EDF prioritizes coders with the earliest deadline using the priority heap.
- **Cooldown handling**: dongles check their release timestamp before being granted.
- **Burnout detection**: monitor checks every 1ms and logs within 10ms of actual burnout.
- **Log serialization**: `pause_print` mutex prevents interleaved output.

---

## Thread synchronization mechanisms

- `pause_dongle`: protects each dongle's state (is_taken, release, queue).
- `pause_print`: serializes all printf calls.
- `pause`: protects `simulation_running` and coder state (last_compile, nbr_of_compilations, done).

---

## Memory management

All dynamically allocated memory is released before the program terminates.
The simulation also destroys the initialized mutexes and joins all created threads before cleanup.

---

## Resources

- [POSIX Threads](https://hpc-tutorials.llnl.gov/posix/)
- [Dining Philosophers Problem](https://en.wikipedia.org/wiki/Dining_philosophers_problem)
- AI was used to explain concurrency concepts, and review code logic.