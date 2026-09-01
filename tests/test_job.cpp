#include <cassert>
#include <iostream>
#include "Job.h"
#include "JobErrors.h"

void test_successful_job_completes() {
    Job job(1, 5, [] {});
    job.run();
    assert(job.state() == JobState::COMPLETED);
    std::cout << "test_successful_job_completes passed\n";
}

void test_transient_failure_retries_then_fails() {
    Job job(2, 5, [] { throw TransientError("temp"); }, /*maxRetries=*/1);
    job.run(); // fail 1 -> RETRYING (retryCount=1, maxRetries=1)
    assert(job.state() == JobState::RETRYING);

    // Simulate Scheduler re-running it after requeue
    job.run(); // fail 2 -> retryCount=2 > maxRetries=1 -> FAILED
    assert(job.state() == JobState::FAILED);
    std::cout << "test_transient_failure_retries_then_fails passed\n";
}

void test_permanent_failure_is_terminal_immediately() {
    Job job(3, 5, [] { throw PermanentError("bad input"); });
    job.run();
    assert(job.state() == JobState::FAILED);
    assert(job.retryCount() == 0); // permanent errors never touch retry count
    std::cout << "test_permanent_failure_is_terminal_immediately passed\n";
}

int main() {
    test_successful_job_completes();
    test_transient_failure_retries_then_fails();
    test_permanent_failure_is_terminal_immediately();
    std::cout << "All tests passed.\n";
    return 0;
}
