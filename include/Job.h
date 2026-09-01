#pragma once
#include <functional>

enum class JobState { PENDING, RUNNING, COMPLETED, FAILED, RETRYING };

class Job {
public:
    // groupId is used only by RoundRobinStrategy to fairly cycle between
    // different job "sources" (e.g. different clients submitting jobs).
    // Jobs from the same groupId share a sub-queue; unrelated jobs default to group 0.
    Job(int id, int priority, std::function<void()> task,
        int maxRetries = 3, int groupId = 0);

    void run();                 // executes task_, catches exceptions, updates state
    bool shouldRetry() const;   // true if last failure was transient AND budget remains

    int id() const { return id_; }
    int priority() const { return priority_; }
    int groupId() const { return groupId_; }
    JobState state() const { return state_; }
    int retryCount() const { return retryCount_; }

private:
    int id_;
    int priority_;
    std::function<void()> task_;
    int maxRetries_;
    int groupId_;

    JobState state_ = JobState::PENDING;
    int retryCount_ = 0;
    bool lastFailureWasTransient_ = false;

    void transitionTo(JobState next);
    static bool isLegalTransition(JobState from, JobState to);
};
