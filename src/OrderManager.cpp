
#pragma once

#include <unordered_map>
#include <atomic>
#include <vector>
#include <chrono>
#include <shared_mutex>

#include "Order.hpp"
#include "OrderManager.hpp"
#include <iostream>

OrderManager::OrderManager() : nextOrderID(1) {
    orders.reserve(10000);
}

uint64_t OrderManager::addOrder(uint8_t side, uint32_t price, uint32_t quantity) {
    std::unique_lock<std::shared_mutex> lock(ordersMutex);
    
    Order order;
    order.id = nextOrderID.fetch_add(1, std::memory_order_relaxed);
    order.price = price;
    order.quantity = quantity;
    order.side = side;
    order.remaining = quantity;

    // Set timestamp in nanoseconds
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    order.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    
    orders[order.id] = order;        
    return order.id;
}

size_t OrderManager::addOrderBatch(const std::vector<Order>& orderBatch) {
    std::unique_lock<std::shared_mutex> lock(ordersMutex);

    // Single timestamp for entire batch (performance optimization)
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    uint64_t batch_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    size_t counter = 0;
    for (const Order& ord : orderBatch) {
        Order newOrder = ord;
        newOrder.id = nextOrderID.fetch_add(1, std::memory_order_relaxed);
        newOrder.timestamp_ns = batch_timestamp;
        orders[newOrder.id] = std::move(newOrder);
        counter++;
    }

    return counter;
}

bool OrderManager::cancelOrder(uint64_t orderID) {
    std::unique_lock<std::shared_mutex> lock(ordersMutex);
    
    auto it = orders.find(orderID);
    if (it != orders.end()) {
        orders.erase(it);
        return true;
    }
    return false;
}

size_t OrderManager::getOrderCount() const {
    std::shared_lock<std::shared_mutex> lock(ordersMutex);
    return orders.size();
}

std::vector<Order> OrderManager::getAllOrders() const {
    std::shared_lock<std::shared_mutex> lock(ordersMutex);
    
    std::vector<Order> orderList;
    orderList.reserve(orders.size());

    for (const auto& [id, order]: orders) {
        orderList.push_back(order);
    }
    return orderList;
}
