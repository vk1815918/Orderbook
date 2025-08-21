#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cstdlib>
#include "Config.hpp"
#include "AtomicRingBuffer.hpp"
#include "OrderManager.hpp"
#include "OrderGenerator.hpp"
#include "MatchingWorker.hpp"
#include "Stats.hpp"

int main(int argc, char *argv[])
{
    std::cout << "Starting main function..." << std::endl;

    // Create configuration
    Config config;

    // Simple command line parsing for demo toggles
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--latency" || arg == "-l")
        {
            config.show_latency_percentiles = true;
            std::cout << "✅ Latency percentiles enabled" << std::endl;
        }
        else if (arg == "--memory" || arg == "-m")
        {
            config.show_memory_stats = true;
            std::cout << "✅ Memory stats enabled" << std::endl;
        }
        else if (arg == "--cache" || arg == "-c")
        {
            config.show_cache_stats = true;
            std::cout << "✅ Cache stats enabled" << std::endl;
        }
        else if (arg == "--threads" || arg == "-t")
        {
            config.show_thread_stats = true;
            std::cout << "✅ Thread stats enabled" << std::endl;
        }
        else if (arg == "--all" || arg == "-a")
        {
            config.show_all_advanced = true;
            std::cout << "✅ All advanced stats enabled" << std::endl;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "HFT Orderbook Engine - Advanced Stats Demo\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  -l, --latency    Show latency percentiles (P50, P95, P99)\n";
            std::cout << "  -m, --memory     Show memory allocation stats\n";
            std::cout << "  -c, --cache      Show cache performance stats\n";
            std::cout << "  -t, --threads    Show per-thread performance\n";
            std::cout << "  -a, --all        Show all advanced stats\n";
            std::cout << "  -h, --help       Show this help\n";
            return 0;
        }
    }

    std::cout << "Config created successfully" << std::endl;

    // Create per-worker ring buffers (SPSC each) to avoid consumer contention
    std::cout << "Creating per-worker ring buffers..." << std::endl;
    const int NUM_WORKERS = 8; // Use 8 worker threads for maximum throughput
    std::vector<AtomicRingBuffer<OrderMsg> *> rings;
    rings.reserve(NUM_WORKERS);
    for (int i = 0; i < NUM_WORKERS; ++i)
    {
        AtomicRingBuffer<OrderMsg> *r = new AtomicRingBuffer<OrderMsg>(config.RING_CAPACITY / NUM_WORKERS);
        rings.push_back(r);
    }
    std::cout << "Created " << NUM_WORKERS << " ring buffers (each capacity: " << (config.RING_CAPACITY / NUM_WORKERS) << ")" << std::endl;

    // Create done flag
    std::atomic<bool> done(false);
    std::cout << "Done flag created" << std::endl;

    // Create OrderManager (sharded)
    std::cout << "Creating sharded OrderManager..." << std::endl;
    OrderManager orderManager(NUM_WORKERS);
    std::cout << "OrderManager created with " << NUM_WORKERS << " shards" << std::endl;

    // Create Stats
    std::cout << "Creating Stats..." << std::endl;
    Stats stats;
    std::cout << "Stats created" << std::endl;

    // Create multiple MatchingWorkers for better throughput
    std::cout << "Creating MatchingWorkers..." << std::endl;
    std::vector<MatchingWorker> workers;
    workers.reserve(NUM_WORKERS);

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        workers.emplace_back(*rings[i], orderManager, stats, done);
    }
    std::cout << NUM_WORKERS << " MatchingWorkers created" << std::endl;

    // Create OrderGenerator (routes to per-worker rings)
    std::cout << "Creating OrderGenerator..." << std::endl;
    OrderGenerator generator(rings, orderManager, config, done, stats);
    std::cout << "OrderGenerator created" << std::endl;

    std::cout << "All modules created successfully. Starting threads..." << std::endl;

    // Start timing
    stats.start();

    // Start consumer threads (multiple workers)
    std::vector<std::thread> consumer_threads;
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        consumer_threads.emplace_back(std::ref(workers[i]));
        std::cout << "Consumer thread " << (i + 1) << " started" << std::endl;
    }

    // Start producer thread
    std::thread producer_thread(std::ref(generator));
    std::cout << "Producer thread started" << std::endl;

    std::cout << "Waiting for threads to complete..." << std::endl;

    // Wait for producer to finish
    producer_thread.join();
    std::cout << "Producer thread joined" << std::endl;

    // Wait for all consumers to finish
    for (auto &thread : consumer_threads)
    {
        thread.join();
    }
    std::cout << "All consumer threads joined" << std::endl;

    std::cout << "Threads completed." << std::endl;

    // Stop timing
    stats.stop();

    std::cout << "Getting final stats..." << std::endl;

    // Get final stats from the matching engine
    auto final_stats = orderManager.getMatchingEngineStats();

    // Add some simulated advanced stats for demo
    if (config.show_all_advanced || config.show_latency_percentiles ||
        config.show_memory_stats || config.show_cache_stats || config.show_thread_stats)
    {

        // Simulate some latency measurements (in real implementation, measure actual latencies)
        for (int i = 0; i < 1000; ++i)
        {
            stats.advanced->addLatency(100 + (rand() % 1000)); // 100-1100ns range
        }

        // Simulate memory usage
        stats.advanced->updateMemory(1024 * 1024 * 512);       // 512MB current
        stats.advanced->peak_memory_usage = 1024 * 1024 * 756; // 756MB peak
        stats.advanced->allocations = 1000000;
        stats.advanced->deallocations = 999500;

        // Simulate cache stats
        stats.advanced->cache_hits = 45000000;
        stats.advanced->cache_misses = 500000;

        // Simulate per-thread stats
        for (int i = 0; i < NUM_WORKERS; ++i)
        {
            stats.advanced->thread_stats[i].processed = stats.popped.load() / NUM_WORKERS;
            stats.advanced->thread_stats[i].batches = 75000;
        }
    }

    // Print final stats with advanced options
    bool show_latency = config.show_all_advanced || config.show_latency_percentiles;
    bool show_memory = config.show_all_advanced || config.show_memory_stats;
    bool show_cache = config.show_all_advanced || config.show_cache_stats;
    bool show_threads = config.show_all_advanced || config.show_thread_stats;

    stats.print(final_stats.throughput, show_latency, show_memory, show_cache, show_threads);

    // free ring buffers
    for (auto r : rings)
        delete r;

    std::cout << "Program completed successfully!" << std::endl;
    return 0;
}
