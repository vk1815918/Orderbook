#include <iostream>
#include <chrono>

#include Order.hpp"

int main() {
    std::cout << "High performance orderbook engine starting... \n";

    Order test_order{};

    test_order.id = 1;
    test_order.price = 10000;
    test_order.quantity = 100;
    test_order.side = Side::BUY;

    std::cout << "Created order ID: " << test_order.id << std::endl;

    return 0;

}