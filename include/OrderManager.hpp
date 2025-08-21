#pragma once
#include <unordered_map>
#include <atomic>
#include <vector>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include "Order.hpp"

// Sharded OrderManager: reduces contention by partitioning orders across N shards.
class OrderManager
{
public:
    explicit OrderManager(size_t num_shards = 8);
    ~OrderManager() = default;

    // Core operations
    uint64_t addOrder(uint8_t side, uint32_t price, uint32_t quantity);
    bool cancelOrder(uint64_t orderID);

    // Batch operations
    size_t addOrderBatch(const std::vector<Order> &orderBatch);

    // Queries
    size_t getOrderCount() const;
    std::vector<Order> getAllOrders() const;

    // Stats for matching engine
    struct MatchingStats
    {
        double throughput;
        uint64_t total_orders;
    };
    MatchingStats getMatchingEngineStats() const;

private:
    const size_t num_shards_;
    std::atomic<uint64_t> nextOrderID_;

    // Per-shard storage and locks
    std::vector<std::unordered_map<uint64_t, Order>> shards_;
    std::vector<std::mutex> shard_mutexes_;
};
