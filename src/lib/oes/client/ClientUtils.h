#ifndef OES_CLIENT_CLIENT_UTILS_H
#define OES_CLIENT_CLIENT_UTILS_H

#include <chrono>
#include <iostream>

#include "export.h"

namespace oes::client {

struct OES_CLIENT_EXPORT EpcStatistics {
    double elapsed_sec_ = 0;

    size_t object_count_ = 0;
    size_t object_bytes_ = 0;
    size_t relation_count_ = 0;

    size_t array_count_ = 0;
    size_t array_bytes_ = 0;
    size_t array_chunks_ = 0;
};

struct OES_CLIENT_EXPORT EpcOptions {
    std::chrono::milliseconds timeout_;
    size_t max_batch_size_ = 100;

    // logging/reporting control
    std::ostream* out_ = nullptr; // normal mode
    std::ostream* msg_ = nullptr; // verbose mode
    std::ostream* warn_ = nullptr;
    std::ostream* err_ = nullptr;

    // scoped transaction retries
    unsigned short n_transaction_retries_ = 0;
};

} // namespace oes::client

#endif