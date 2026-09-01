#include "Job.h"
#include "JobErrors.h"
#include <cassert>

Job::Job(int id, int priority, std::function<void()> task, int maxRetries, int groupId)
    : id_(id), priority_(priority), task_(std::move(task)),
      maxRetries_(maxRetries), groupId_(groupId) {}

bool Job::isLegalTransition(JobState from, JobState to) {
    switch (from) {
        case JobState::PENDING:
            return to == JobState::RUNNING;
        case JobState::RUNNING:
            return to == JobState::COMPLETED
                || to == JobState::FAILED
                || to == JobState::RETRYING;
        case JobState::RETRYING:
            return to == JobState::RUNNING
                || to == JobState::FAILED;
        case JobState::COMPLETED:
        case JobState::FAILED:
            return false; // terminal states
    }
    return false;
}

void Job::transitionTo(JobState next) {
    assert(isLegalTransition(state_, next) && "Illegal job state transition");
    state_ = next;
}

void Job::run() {
    transitionTo(JobState::RUNNING);
    try {
        task_();
        transitionTo(JobState::COMPLETED);
    } catch (const TransientError&) {
        lastFailureWasTransient_ = true;
        retryCount_++;
        transitionTo(shouldRetry() ? JobState::RETRYING : JobState::FAILED);
    } catch (const std::exception&) {
        // PermanentError and anything unclassified (bad_alloc, logic_error, etc.)
        lastFailureWasTransient_ = false;
        transitionTo(JobState::FAILED);
    }
}

bool Job::shouldRetry() const {
    return lastFailureWasTransient_ && retryCount_ <= maxRetries_;
}
