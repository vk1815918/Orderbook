#pragma once
#include "MatchingEngine.hpp" // defines OrderIn, SIDE_BUY/SELL

enum class MessageType : uint8_t
{
    ADD_ORDER = 0,
    CANCEL_ORDER = 1
};

// Extend incoming message with routing hint for worker (round-robin/shard)
struct OrderMsg : public OrderIn
{
    uint32_t worker_id = 0;                        // target worker queue
    MessageType msg_type = MessageType::ADD_ORDER; // message type
    uint32_t handle_to_cancel = 0;                 // for cancel messages, which handle to cancel
};
