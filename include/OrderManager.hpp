#pragma once

#include <unordered_map>
#include <atomic>
#include <vector>
#include <chrono>

#include "Order.hpp"


// class OrderManager {
// private:

//     std::unordered_map<uint64_t, Order> orders;
//     std::atomic<uint64_t> nextOrderID;

// public:

//     OrderManager();
//     ~OrderManager();

//     uint64_t addOrder(uint8_t side, uint64_t price, uint32_t quantity) {
//         Order* order = new Order();
//         order->id = nextOrderID;
//         order->price = price;
//         order->side = side;
//         order->quantity = quantity;
//         order->remaining = quantity; // what is the point of a separate remaining?
//         // set ts in nanoseconds
//         orders[order->id] = *order;
//     }

//     bool cancelOrder(uint64_t orderID) {
//         delete orders[orderID];
//     }



// };
