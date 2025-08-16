#pragma once
#include <unordered_map>
#include <atomic>
#include <vector>
#include <chrono>
#include <shared_mutex>
#include "Order.hpp"

class OrderManager {
private:
    mutable std::shared_mutex ordersMutex;
    std::unordered_map<uint64_t, Order> orders;
    std::atomic<uint64_t> nextOrderID;

public:
    OrderManager();
    ~OrderManager() = default;

    // Core operations
    uint64_t addOrder(uint8_t side, uint32_t price, uint32_t quantity);
    bool cancelOrder(uint64_t orderID);
    
    // Batch operations
    size_t addOrderBatch(const std::vector<Order>& orderBatch);
    
    // Queries
    size_t getOrderCount() const;
    std::vector<Order> getAllOrders() const;
};
