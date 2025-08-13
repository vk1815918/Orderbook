#pragma once
#include <cstdint>

struct Order {

    uint64_t id;
    uint64_t timestamp_ns; // nanoseconds since epoch
    uint32_t quantity; // 
    uint32_t remaining;
    uint32_t price;
    uint8_t side;

};