#pragma once
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#include "Job.h"
#include "SchedulingStrategy.h"

class Scheduler {
public:
    Scheduler(std::unique_ptr<SchedulingStrategy> strategy, size_t numWorkers);
    ~Scheduler();

    void submit(std::shared_ptr<Job> job);
    void start();
    void shutdown();

    JobState statusOf(int jobId) const;

private:
    std::unique_ptr<SchedulingStrategy> strategy_;
    std::unordered_map<int, std::shared_ptr<Job>> jobsById_;
    std::vector<std::thread> workers_;
    size_t numWorkers_;

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool stopping_ = false;
    bool started_ = false;

    void workerLoop();
};
