#pragma once
// #include <string>
// #include <chrono>
#include <cstdint>

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1
};

struct Order {

    uint64_t id;
    uint64_t timestamp;
    uint32_t quantity;
    uint32_t remaining;
    uint32_t price;

    Side side;
    OrderType type;

    Order* next = nullptr;
    Order* prev = nullptr;

};