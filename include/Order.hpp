#pragma once
#include <string>
#include <chrono>

enum class OrderSide {BUY, SELL};

struct Order {
    int id;
    OrderSide side;
    double price;
    int quantity;
    std::chrono::high_resolution_clock::time_point timestamp;

    Order(int id, OrderSide side, double price, int quantity);

};