
#include <vector>
#include <chrono>

#include "Order.hpp"
#include "OrderManager.hpp"
#include <iostream>

OrderManager::OrderManager(size_t num_shards)
    : num_shards_(num_shards), nextOrderID_(1), shards_(num_shards), shard_mutexes_(num_shards)
{
    for (size_t i = 0; i < num_shards_; ++i)
        shards_[i].reserve(1024);
}

uint64_t OrderManager::addOrder(uint8_t side, uint32_t price, uint32_t quantity)
{
    uint64_t id = nextOrderID_.fetch_add(1, std::memory_order_relaxed);
    Order order;
    order.id = id;
    order.price = price;
    order.quantity = quantity;
    order.side = side;
    order.remaining = quantity;

    // timestamp
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    order.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    size_t shard = id % num_shards_;
    {
        std::scoped_lock lock(shard_mutexes_[shard]);
        shards_[shard].emplace(id, std::move(order));
    }
    return id;
}

size_t OrderManager::addOrderBatch(const std::vector<Order> &orderBatch)
{
    // single timestamp for the batch
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    uint64_t batch_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    size_t counter = 0;
    for (const Order &ord : orderBatch)
    {
        uint64_t id = nextOrderID_.fetch_add(1, std::memory_order_relaxed);
        Order newOrder = ord;
        newOrder.id = id;
        newOrder.timestamp_ns = batch_timestamp;
        size_t shard = id % num_shards_;
        {
            std::scoped_lock lock(shard_mutexes_[shard]);
            shards_[shard].emplace(id, std::move(newOrder));
        }
        ++counter;
    }
    return counter;
}

bool OrderManager::cancelOrder(uint64_t orderID)
{
    size_t shard = orderID % num_shards_;
    std::scoped_lock lock(shard_mutexes_[shard]);
    auto it = shards_[shard].find(orderID);
    if (it != shards_[shard].end())
    {
        shards_[shard].erase(it);
        return true;
    }
    return false;
}

size_t OrderManager::getOrderCount() const
{
    size_t total = 0;
    for (size_t i = 0; i < num_shards_; ++i)
    {
        std::scoped_lock lock((std::mutex &)shard_mutexes_[i]);
        total += shards_[i].size();
    }
    return total;
}

OrderManager::MatchingStats OrderManager::getMatchingEngineStats() const
{
    MatchingStats stats{};
    stats.total_orders = getOrderCount();
    stats.throughput = 0.0;
    return stats;
}

std::vector<Order> OrderManager::getAllOrders() const
{
    std::vector<Order> out;
    out.reserve(getOrderCount());
    for (size_t i = 0; i < num_shards_; ++i)
    {
        std::scoped_lock lock((std::mutex &)shard_mutexes_[i]);
        for (auto &kv : shards_[i])
            out.push_back(kv.second);
    }
    return out;
}
