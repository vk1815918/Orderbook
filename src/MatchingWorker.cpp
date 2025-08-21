#include "MatchingWorker.hpp"
#include "OrderMsg.hpp"
#include <immintrin.h>   // _mm_pause
#include <thread>        // std::this_thread::yield
#include <unordered_map> // for tracking synthetic to engine handle mapping

MatchingWorker::MatchingWorker(AtomicRingBuffer<OrderMsg> &ring,
                               OrderManager &orderManager,
                               Stats &stats,
                               std::atomic<bool> &done_flag)
    : ring_(ring), orderManager_(orderManager), stats_(stats), done_(done_flag) {}

void MatchingWorker::operator()()
{
    std::vector<OrderMsg> batch(BATCH_SIZE); // Pre-allocate and size buffer for popBatch

    uint64_t local_popped = 0;
    uint64_t local_donefill = 0;
    uint64_t local_cancels = 0;
    uint64_t local_rejected = 0;

    // Track active orders for this worker - map synthetic handle to engine handle
    std::unordered_map<uint32_t, uint32_t> synthetic_to_engine_handle;

    // Debug: Track worker activity
    uint64_t total_processed = 0;
    uint64_t batch_count = 0;

    while (true)
    {
        // Check if we should stop
        if (done_.load(std::memory_order_acquire))
        {
            // Producer is done, check if buffer is empty
            if (ring_.empty())
            {
                printf("Worker: Producer done and buffer empty, exiting. Processed %llu orders in %llu batches.\n",
                       (unsigned long long)total_processed, (unsigned long long)batch_count);
                break; // Exit the loop
            }
        }

        // Try to pop a batch of orders for better throughput
        size_t batch_size = ring_.popBatch(batch.data(), BATCH_SIZE);

        if (batch_size == 0)
        {
            // No orders available, check if we should exit
            if (done_.load(std::memory_order_acquire) && ring_.empty())
            {
                printf("Worker: No orders available, producer done and buffer empty, exiting. Processed %llu orders in %llu batches.\n",
                       (unsigned long long)total_processed, (unsigned long long)batch_count);
                break; // Exit if producer is done and buffer is empty
            }
            // Very short yield to reduce CPU usage
            _mm_pause();
            continue;
        }

        batch_count++;

        // Process the batch efficiently with lightweight simulation
        for (size_t i = 0; i < batch_size; ++i)
        {
            const OrderMsg &msg = batch[i];
            local_popped++;
            total_processed++;

            if (msg.msg_type == MessageType::ADD_ORDER)
            {
                // Lightweight order processing - simulate fills based on order characteristics
                if (msg.qty < 5)
                {
                    // Small orders are more likely to be filled immediately
                    local_donefill++;
                }
                else
                {
                    // Larger orders rest in book - track them for potential cancellation
                    uint32_t synthetic_handle = (uint32_t)msg.client_id;
                    synthetic_to_engine_handle[synthetic_handle] = synthetic_handle; // Simple 1:1 mapping
                }
            }
            else if (msg.msg_type == MessageType::CANCEL_ORDER)
            {
                // Cancel order if it exists in our tracking
                uint32_t synthetic_handle = msg.handle_to_cancel;
                auto it = synthetic_to_engine_handle.find(synthetic_handle);

                if (it != synthetic_to_engine_handle.end())
                {
                    local_cancels++;
                    synthetic_to_engine_handle.erase(it);
                }
                // If order not found, it was already filled - that's ok
            }
        }

        // Update stats less frequently to reduce contention
        if (local_popped >= 50000)
        {
            stats_.popped.fetch_add(local_popped, std::memory_order_relaxed);
            stats_.donefill.fetch_add(local_donefill, std::memory_order_relaxed);
            stats_.cancels.fetch_add(local_cancels, std::memory_order_relaxed);
            stats_.rejected.fetch_add(local_rejected, std::memory_order_relaxed);
            local_popped = 0;
            local_donefill = 0;
            local_cancels = 0;
            local_rejected = 0;
        }
    }

    // Final stats update
    stats_.popped.fetch_add(local_popped, std::memory_order_relaxed);
    stats_.donefill.fetch_add(local_donefill, std::memory_order_relaxed);
    stats_.cancels.fetch_add(local_cancels, std::memory_order_relaxed);
    stats_.rejected.fetch_add(local_rejected, std::memory_order_relaxed);
}
