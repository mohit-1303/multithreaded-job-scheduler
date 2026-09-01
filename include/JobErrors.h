#pragma once
#include <stdexcept>
#include <string>

// Base for all job-related errors.
class JobError : public std::runtime_error {
public:
    explicit JobError(const std::string& msg) : std::runtime_error(msg) {}
};

// Retryable: environmental/temporary failure. Retrying might succeed.
class TransientError : public JobError {
public:
    explicit TransientError(const std::string& msg) : JobError(msg) {}
};

// Not retryable: something is actually wrong with the task/input.
class PermanentError : public JobError {
public:
    explicit PermanentError(const std::string& msg) : JobError(msg) {}
};
