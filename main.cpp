#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "Config.hpp"
#include "AtomicRingBuffer.hpp"
#include "OrderManager.hpp"
#include "OrderGenerator.hpp"
#include "MatchingWorker.hpp"
#include "Stats.hpp"

int main() {
    std::cout << "Starting main function..." << std::endl;

    // Create configuration
    Config config;
    std::cout << "Config created successfully" << std::endl;

    // Create ring buffer
    std::cout << "Creating ring buffer..." << std::endl;
    AtomicRingBuffer<OrderMsg> ring(config.RING_CAPACITY);
    std::cout << "Ring buffer created, capacity: " << config.RING_CAPACITY << std::endl;

    // Create done flag
    std::atomic<bool> done(false);
    std::cout << "Done flag created" << std::endl;

    // Create OrderManager
    std::cout << "Creating OrderManager..." << std::endl;
    OrderManager orderManager;
    std::cout << "OrderManager created" << std::endl;

    // Create Stats
    std::cout << "Creating Stats..." << std::endl;
    Stats stats;
    std::cout << "Stats created" << std::endl;

    // Create multiple MatchingWorkers for better throughput
    std::cout << "Creating MatchingWorkers..." << std::endl;
    const int NUM_WORKERS = 8; // Use 8 worker threads for maximum throughput
    std::vector<MatchingWorker> workers;
    workers.reserve(NUM_WORKERS);
    
    for (int i = 0; i < NUM_WORKERS; i++) {
        workers.emplace_back(ring, orderManager, stats, done);
    }
    std::cout << NUM_WORKERS << " MatchingWorkers created" << std::endl;

    // Create OrderGenerator
    std::cout << "Creating OrderGenerator..." << std::endl;
    OrderGenerator generator(ring, orderManager, config, done, stats);
    std::cout << "OrderGenerator created" << std::endl;

    std::cout << "All modules created successfully. Starting threads..." << std::endl;

    // Start timing
    stats.start();

    // Start consumer threads (multiple workers)
    std::vector<std::thread> consumer_threads;
    for (int i = 0; i < NUM_WORKERS; i++) {
        consumer_threads.emplace_back(std::ref(workers[i]));
        std::cout << "Consumer thread " << (i+1) << " started" << std::endl;
    }

    // Start producer thread
    std::thread producer_thread(std::ref(generator));
    std::cout << "Producer thread started" << std::endl;

    std::cout << "Waiting for threads to complete..." << std::endl;

    // Wait for producer to finish
    producer_thread.join();
    std::cout << "Producer thread joined" << std::endl;

    // Wait for all consumers to finish
    for (auto& thread : consumer_threads) {
        thread.join();
    }
    std::cout << "All consumer threads joined" << std::endl;

    std::cout << "Threads completed." << std::endl;

    // Stop timing
    stats.stop();

    std::cout << "Getting final stats..." << std::endl;

    // Get final stats from the matching engine
    auto final_stats = orderManager.getMatchingEngineStats();
    
    // Print final stats
    stats.print(final_stats.throughput);

    std::cout << "Program completed successfully!" << std::endl;
    return 0;
}
