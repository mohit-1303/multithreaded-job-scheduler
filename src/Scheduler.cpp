#include "Scheduler.h"
#include <stdexcept>

Scheduler::Scheduler(std::unique_ptr<SchedulingStrategy> strategy, size_t numWorkers)
    : strategy_(std::move(strategy)), numWorkers_(numWorkers) {}

Scheduler::~Scheduler() {
    if (started_ && !stopping_) {
        shutdown();
    }
}

void Scheduler::submit(std::shared_ptr<Job> job) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        jobsById_[job->id()] = job;
        strategy_->addJob(job);
    }
    cv_.notify_one();
}

void Scheduler::start() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (started_) return;
    started_ = true;
    for (size_t i = 0; i < numWorkers_; ++i) {
        workers_.emplace_back(&Scheduler::workerLoop, this);
    }
}

void Scheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void Scheduler::workerLoop() {
    while (true) {
        std::shared_ptr<Job> job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return stopping_ || !strategy_->empty(); });

            if (stopping_ && strategy_->empty()) {
                return; // drain remaining work before exiting; nothing left, so stop
            }

            job = strategy_->nextJob();
            if (!job) continue; // spurious wake or race: loop and wait again
        }

        job->run(); // executed OUTSIDE the lock — this is deliberate, see note below

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (job->state() == JobState::RETRYING) {
                strategy_->requeueJob(job);
            }
        }
        cv_.notify_one(); // in case a retry just re-entered the queue
    }
}

JobState Scheduler::statusOf(int jobId) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = jobsById_.find(jobId);
    if (it == jobsById_.end()) {
        throw std::out_of_range("No job with that id");
    }
    return it->second->state();
}
