#pragma once
#include <memory>
#include <deque>
#include <queue>
#include <vector>
#include <unordered_map>
#include "Job.h"

class SchedulingStrategy {
public:
    virtual ~SchedulingStrategy() = default;
    virtual void addJob(std::shared_ptr<Job> job) = 0;      // new submission
    virtual void requeueJob(std::shared_ptr<Job> job) = 0;  // retry resubmission
    virtual std::shared_ptr<Job> nextJob() = 0;              // nullptr if empty
    virtual bool empty() const = 0;
};

// --- Priority: always run the highest-priority job available.
// Retries keep their original priority, so requeueJob == addJob.
class PriorityStrategy : public SchedulingStrategy {
public:
    void addJob(std::shared_ptr<Job> job) override { queue_.push(job); }
    void requeueJob(std::shared_ptr<Job> job) override { queue_.push(job); }

    std::shared_ptr<Job> nextJob() override {
        if (queue_.empty()) return nullptr;
        auto job = queue_.top();
        queue_.pop();
        return job;
    }

    bool empty() const override { return queue_.empty(); }

private:
    struct Compare {
        bool operator()(const std::shared_ptr<Job>& a, const std::shared_ptr<Job>& b) const {
            return a->priority() < b->priority(); // max-heap: highest priority first
        }
    };
    std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>, Compare> queue_;
};

// --- FCFS: strict arrival order. Retries jump to the front, since they've
// already waited once and shouldn't lose their place to brand-new jobs.
class FCFSStrategy : public SchedulingStrategy {
public:
    void addJob(std::shared_ptr<Job> job) override { queue_.push_back(job); }
    void requeueJob(std::shared_ptr<Job> job) override { queue_.push_front(job); }

    std::shared_ptr<Job> nextJob() override {
        if (queue_.empty()) return nullptr;
        auto job = queue_.front();
        queue_.pop_front();
        return job;
    }

    bool empty() const override { return queue_.empty(); }

private:
    std::deque<std::shared_ptr<Job>> queue_;
};

// --- Round Robin: fair cycling across job "groups" (e.g. different clients),
// so one group flooding the scheduler with jobs can't starve another group.
// Jobs within a group still run atomically to completion (see Job::run) —
// fairness comes from which group gets picked next, not from interrupting a job.
class RoundRobinStrategy : public SchedulingStrategy {
public:
    void addJob(std::shared_ptr<Job> job) override {
        int group = job->groupId();
        bool groupWasEmpty = groupQueues_[group].empty();
        groupQueues_[group].push_back(job);
        if (groupWasEmpty) groupOrder_.push_back(group);
        ++size_;
    }

    void requeueJob(std::shared_ptr<Job> job) override {
        int group = job->groupId();
        bool groupWasEmpty = groupQueues_[group].empty();
        groupQueues_[group].push_front(job);
        if (groupWasEmpty) groupOrder_.push_front(group);
        ++size_;
    }

    std::shared_ptr<Job> nextJob() override {
        if (groupOrder_.empty()) return nullptr;
        int group = groupOrder_.front();
        groupOrder_.pop_front();

        auto& q = groupQueues_[group];
        auto job = q.front();
        q.pop_front();
        --size_;

        if (!q.empty()) groupOrder_.push_back(group); // group still has work: give it another turn later
        return job;
    }

    bool empty() const override { return size_ == 0; }

private:
    std::unordered_map<int, std::deque<std::shared_ptr<Job>>> groupQueues_;
    std::deque<int> groupOrder_;
    size_t size_ = 0;
};
