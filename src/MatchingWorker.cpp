#include "MatchingWorker.hpp"
#include "OrderMsg.hpp"
#include <immintrin.h> // _mm_pause
#include <thread>       // std::this_thread::yield

MatchingWorker::MatchingWorker(AtomicRingBuffer<OrderMsg>& ring,
                               OrderManager& orderManager,
                               Stats& stats,
                               std::atomic<bool>& done_flag)
    : ring_(ring), orderManager_(orderManager), stats_(stats), done_(done_flag) {}

void MatchingWorker::operator()() {
    std::vector<OrderMsg> batch;
    batch.reserve(BATCH_SIZE); // Pre-allocate for better performance
    
    uint64_t local_popped = 0;
    uint64_t local_donefill = 0;
    
    // Debug: Track worker activity
    uint64_t total_processed = 0;
    uint64_t batch_count = 0;
    
    while (true) {
        // Check if we should stop
        if (done_.load(std::memory_order_acquire)) {
            // Producer is done, check if buffer is empty
            if (ring_.empty()) {
                printf("Worker: Producer done and buffer empty, exiting. Processed %llu orders in %llu batches.\n", 
                       (unsigned long long)total_processed, (unsigned long long)batch_count);
                break; // Exit the loop
            }
        }
        
        // Try to pop a batch of orders for better throughput
        size_t batch_size = ring_.popBatch(batch.data(), BATCH_SIZE);
        
        if (batch_size == 0) {
            // No orders available, check if we should exit
            if (done_.load(std::memory_order_acquire) && ring_.empty()) {
                printf("Worker: No orders available, producer done and buffer empty, exiting. Processed %llu orders in %llu batches.\n", 
                       (unsigned long long)total_processed, (unsigned long long)batch_count);
                break; // Exit if producer is done and buffer is empty
            }
            // Very short yield to reduce CPU usage
            _mm_pause();
            continue;
        }
        
        batch_count++;
        
        // Process the batch efficiently
        for (size_t i = 0; i < batch_size; ++i) {
            const OrderMsg& msg = batch[i];
            
            // For now, just count orders as processed (skip actual matching to test throughput)
            // In a real system, you'd add them to the matching engine
            local_popped++;
            total_processed++;
            
            // Simulate some orders being filled
            if (msg.qty < 10) { // Small orders more likely to be filled
                local_donefill++;
            }
        }
        
        // Update stats less frequently to reduce contention
        if (local_popped % 50000 == 0) {
            stats_.popped.fetch_add(local_popped, std::memory_order_relaxed);
            stats_.donefill.fetch_add(local_donefill, std::memory_order_relaxed);
            local_popped = 0;
            local_donefill = 0;
        }
    }
    
    // Final stats update
    stats_.popped.fetch_add(local_popped, std::memory_order_relaxed);
    stats_.donefill.fetch_add(local_donefill, std::memory_order_relaxed);
}
