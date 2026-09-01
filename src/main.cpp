#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "Job.h"
#include "JobErrors.h"
#include "SchedulingStrategy.h"
#include "Scheduler.h"

int main() {
    auto strategy = std::make_unique<PriorityStrategy>();
    Scheduler scheduler(std::move(strategy), /*numWorkers=*/3);

    scheduler.submit(std::make_shared<Job>(1, /*priority=*/5, [] {
        std::cout << "Job 1: doing normal work\n";
    }));

    scheduler.submit(std::make_shared<Job>(2, /*priority=*/10, [] {
        std::cout << "Job 2: hit a transient failure\n";
        throw TransientError("temporary resource unavailable");
    }, /*maxRetries=*/2));

    scheduler.submit(std::make_shared<Job>(3, /*priority=*/1, [] {
        std::cout << "Job 3: hit a permanent failure\n";
        throw PermanentError("invalid input, retry won't help");
    }));

    scheduler.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    scheduler.shutdown();

    for (int id : {1, 2, 3}) {
        std::cout << "Job " << id << " final state: "
                  << static_cast<int>(scheduler.statusOf(id)) << "\n";
    }
    return 0;
}
