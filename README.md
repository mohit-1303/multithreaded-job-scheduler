# TaskFlow

**TaskFlow** is a modern C++17 multi-threaded job scheduling engine featuring pluggable scheduling algorithms, explicit state machine lifecycle tracking, automatic retry policies with transient/permanent error classification, and graceful thread pool draining.

---

## Architecture Overview

```
                          ┌───────────────────────┐
                          │    Scheduler::submit  │
                          └──────────┬────────────┘
                                     │
                                     ▼
                          ┌───────────────────────┐
                          │  SchedulingStrategy   │
                          │ (Priority/FCFS/RR)    │
                          └──────────┬────────────┘
                                     │
                                     ▼
                ┌────────────────────┴────────────────────┐
                │             Worker Thread Pool          │
                │                                         │
                │   [Worker 1]    [Worker 2]   [Worker 3] │
                └────────┬────────────┬───────────┬───────┘
                         │            │           │
                         ▼            ▼           ▼
                      Job::run() (Executed outside lock)
                                      │
            ┌─────────────────────────┼─────────────────────────┐
            │                         │                         │
            ▼                         ▼                         ▼
       [Success]              [TransientError]           [PermanentError]
      COMPLETED                 Retry budget?                 FAILED
                              ┌───────┴───────┐
                              ▼               ▼
                          RETRYING          FAILED
                      (Requeued to Strat)
```

---

## Core Components

### 1. Job State Machine & Lifecycle (`Job.h`, `Job.cpp`)
Every job progresses through an explicit, validated state transition model:
- `PENDING` $\to$ `RUNNING`
- `RUNNING` $\to$ `COMPLETED` | `RETRYING` | `FAILED`
- `RETRYING` $\to$ `RUNNING` | `FAILED`
- Terminal states: `COMPLETED`, `FAILED`

Transitions are validated against illegal mutations via `transitionTo()`.

### 2. Error Classification & Retry Policy (`JobErrors.h`)
- **`TransientError`**: Represents temporary issues (network timeout, lock contention, temporary resource exhaustion). Jobs failing with a transient error increment their retry count and transition to `RETRYING` as long as `retryCount <= maxRetries`.
- **`PermanentError`** (and standard exceptions): Represents unrecoverable failures (invalid input, schema mismatch, logic error). Jobs fail immediately without consuming retry attempts.

### 3. Pluggable Scheduling Strategies (`SchedulingStrategy.h`)
- **`PriorityStrategy`**: Max-heap (`std::priority_queue`) prioritizing jobs with highest numeric priority value. Requeued jobs preserve their original priority.
- **`FCFSStrategy`**: FIFO order using `std::deque`. Requeued retry jobs jump to the front (`push_front`) to avoid starvation after already waiting once.
- **`RoundRobinStrategy`**: Fair-share scheduling grouped by `groupId` (e.g., client/tenant ID) to prevent high-volume clients from starving low-volume clients.

### 4. Concurrency & Synchronization (`Scheduler.h`, `Scheduler.cpp`)
- **Fine-Grained Locking**: Locks (`std::mutex`) are held strictly for queue management and state inspection. `job->run()` is executed **outside the lock**, enabling true multi-core parallel job execution.
- **Condition Variable Coordination**: `std::condition_variable cv_` coordinates worker sleep/wakeups on submissions, requeues, and shutdowns.
- **Graceful Shutdown**: `shutdown()` transitions the scheduler to stopping state and drains any remaining jobs in the strategy queues before worker threads join.

---

## Project Structure

```
TaskFlow/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── include/
│   ├── Job.h
│   ├── JobErrors.h
│   ├── SchedulingStrategy.h
│   └── Scheduler.h
├── src/
│   ├── Job.cpp
│   ├── Scheduler.cpp
│   └── main.cpp
└── tests/
    └── test_job.cpp
```

---

## Building and Running

### Prerequisites
- CMake 3.10+
- C++17 compatible compiler (GCC, Clang, or MSVC)
- POSIX / Win32 Threads support

### Build Commands
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Run Demo
```bash
./taskflow       # On Windows: .\taskflow.exe or .\Debug\taskflow.exe
```

### Run Unit Tests
```bash
./taskflow_tests # On Windows: .\taskflow_tests.exe or .\Debug\taskflow_tests.exe
```

---

## Design Decisions & Deep Dive

### Why execute `job->run()` outside the mutex lock?
Holding the scheduler mutex while executing user tasks would serialize execution, degrading an $N$-worker thread pool into single-threaded execution. By releasing `mtx_` prior to `job->run()` and only re-acquiring it during result inspection and requeuing, worker threads achieve full parallelism.

### Why separate `start()` from the constructor?
Separating thread instantiation from object construction prevents race conditions where worker threads begin polling and accessing partially constructed member state before construction completes.

### Why distinguish `addJob()` and `requeueJob()`?
Different scheduling strategies handle retries differently:
- In **FCFS**, retried jobs should jump to the front of the queue because they have already spent time waiting and should not be penalized by new arrivals.
- In **Priority**, retried jobs re-enter at their configured priority rank.
- In **RoundRobin**, retried jobs get placed at the front of their group queue.
