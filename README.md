# TaskFlow — C++ Job Scheduling System

A multithreaded job scheduler in modern C++, built to demonstrate core systems
and OOP fundamentals: state machines, the Strategy pattern, thread-safe
producer/consumer synchronization, and RAII-based resource ownership.

## What it does

Jobs are submitted to a `Scheduler`, which hands them to a pool of worker
threads according to a pluggable scheduling policy (Priority, FCFS, or Round
Robin). Failed jobs are retried automatically if their failure was
classified as transient; permanent failures terminate immediately.

## Architecture

```
Job              — owns its state machine and retry policy
SchedulingStrategy — interface; Priority/FCFS/RoundRobin implementations
Scheduler        — owns the thread pool, mutex/condvar synchronization,
                    and delegates ordering decisions to a SchedulingStrategy
JobErrors        — TransientError / PermanentError hierarchy
```

## Design decisions

**Strategy pattern for scheduling algorithms.**
`Scheduler` doesn't know or care whether it's running Priority, FCFS, or
Round Robin — it only calls `addJob` / `nextJob` / `requeueJob` on whatever
`SchedulingStrategy` it was given. This keeps `Scheduler` (thread management,
synchronization) fully decoupled from ordering policy, and means adding a new
scheduling algorithm never touches `Scheduler` at all.

**Explicit state machine, not an implicit enum.**
Every legal transition is enumerated in `Job::isLegalTransition`, and illegal
transitions trip an assertion rather than silently corrupting state. This
was deliberate: a job going `COMPLETED → RUNNING` should be impossible by
construction, not just "shouldn't happen in practice."

**Round Robin fairness without preemption.**
Jobs run atomically to completion — `std::function<void()>` has no natural
way to pause and resume mid-execution, and safely interrupting arbitrary
running code isn't something C++ threads support. So Round Robin here means
fair *ordering* across job groups (no single group can starve another), not
true time-sliced preemption of a running job. This is a conscious scope
decision: true preemptive scheduling would require jobs to expose resumable,
chunked work, which is a materially bigger feature than this project needs.

**Two-tier exception hierarchy for retry classification.**
`TransientError` (environmental, retry might help) vs. `PermanentError`
(the task itself is wrong, retrying won't fix it) lets `Job::run()` decide
retry eligibility by `catch` type rather than inspecting error messages or
codes. Anything not explicitly classified (`std::bad_alloc`, unexpected
`std::exception`s) is treated as permanent by default — an error the system
didn't anticipate shouldn't be assumed safe to retry.

**Retried jobs get front-of-queue priority.**
A job that already waited once and failed transiently shouldn't lose its
place to brand-new arrivals. For `PriorityStrategy` this is automatic
(priority already governs order); for `FCFSStrategy` and `RoundRobinStrategy`
it's implemented explicitly via `requeueJob`.

**Job execution happens outside the scheduler's lock.**
`workerLoop` releases `mtx_` before calling `job->run()` and only
re-acquires it to check the result and requeue if needed. Holding the lock
during job execution would serialize all workers behind whichever one is
currently running a job, defeating the purpose of a thread pool.

**Smart pointers reflect actual ownership.**
`shared_ptr<Job>` — both the strategy's internal queue and `Scheduler`'s
`jobsById_` map hold references to the same job simultaneously, so shared
ownership is correct, not just convenient. `unique_ptr<SchedulingStrategy>`
— exactly one `Scheduler` owns its strategy for its whole lifetime.


## Build & run

```bash
mkdir build && cd build
cmake ..
make
./taskflow
./taskflow_tests
```

Requires a C++17 compiler and CMake ≥ 3.10.
