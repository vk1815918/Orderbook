// MatchingEngine.hpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <cassert>
#include <cstring> // memset
#include <bit> // this is countr_zero/countl_zero from C++20

// helpful branch prediction micro optimization
#ifndef likely
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// incoming order message input
struct OrderIn {
    uint64_t client_id; // who sent it (passthrough)
    uint32_t price_tick; // price tick (0..MAX_TICKS-1)
    uint32_t qty; // >0
    uint8_t  side; // 0=BUY, 1=SELL
    uint8_t  flags; // bit0: IOC, bit1: FOK  (customize as needed)
    uint16_t _pad{0}; // padding so struct is aligned
};

enum : uint8_t { SIDE_BUY = 0, SIDE_SELL = 1 };

template <uint32_t MAX_TICKS, uint32_t MAX_ORDERS, uint32_t WORD_BITS = 64>
class MatchingEngine {
    static_assert(MAX_TICKS >= 2, "need at least two ticks");
    static_assert(WORD_BITS == 64, "WORD_BITS must be 64");
    static constexpr uint32_t WORDS = (MAX_TICKS + WORD_BITS - 1u) / WORD_BITS;

    // intrusive order node (it resides in a contiguous pool)
    struct OrderNode {
        uint32_t id; // engine handle (index into handle_)
        uint32_t price_tick;
        uint32_t qty; // remaining
        uint32_t next_idx; // index in pool_ (NIL if none)
        uint32_t prev_idx; // index in pool_ (NIL if none)
        uint8_t  side; // store SIDE_BUY/SIDE_SELL
        uint8_t  _pad[3]{};
    };

    static constexpr uint32_t NIL      = 0xFFFFFFFFu;
    static constexpr uint32_t NO_PRICE = 0xFFFFFFFFu;
    static constexpr uint32_t DONE_FILL= 0xFFFFFFFEu;

    struct PriceLevel {
        uint32_t head{NIL};
        uint32_t tail{NIL};
        uint32_t total_qty{0}; // book-keeping (not used in hot path decisions)
    };

public:
    MatchingEngine() { reset(); }

    // Clear book and pool (not thread-safe. call on init/reset only)
    void reset() {
        // order pool free-list
        free_head_ = 0;
        for (uint32_t i = 0; i < MAX_ORDERS; ++i) {
            pool_[i].next_idx = i + 1;    // link free list
            pool_[i].prev_idx = NIL;
            pool_[i].qty = 0;
            handles_[i] = NIL;            // mark handle slot unused
        }
        pool_[MAX_ORDERS - 1].next_idx = NIL;

        // clear price levels and bitsets
        std::memset(bids_bits_.data(), 0, sizeof(uint64_t) * WORDS);
        std::memset(asks_bits_.data(), 0, sizeof(uint64_t) * WORDS);
        for (uint32_t i = 0; i < MAX_TICKS; ++i) { bids_[i] = PriceLevel{}; asks_[i] = PriceLevel{}; }

        best_bid_ = NO_PRICE;
        best_ask_ = NO_PRICE;

        total_trades_ = 0;
        total_volume_ = 0;
        next_handle_  = 0;
    }

    // Use to add a limit order. Returns engine handle (0..MAX_ORDERS-1) on rest, DONE_FILL if fully executed, or NIL on reject.
    inline uint32_t add_limit(const OrderIn& in) {
        if (unlikely(in.qty == 0 || in.price_tick >= MAX_TICKS)) return NIL;

        uint32_t remaining = in.qty;

        if (in.side == SIDE_BUY) {
            // Cross against best ask while price allows
            while (remaining && best_ask_ != NO_PRICE && best_ask_ <= in.price_tick) {
                uint32_t tick = best_ask_;
                PriceLevel& lvl = asks_[tick];

                while (remaining && lvl.head != NIL) {
                    uint32_t idx = lvl.head;
                    OrderNode& maker = pool_[idx];

                    uint32_t trade = (remaining < maker.qty) ? remaining : maker.qty;
                    maker.qty -= trade;
                    remaining -= trade;
                    lvl.total_qty -= trade;

                    ++total_trades_;
                    total_volume_ += trade;

                    if (maker.qty == 0) {
                        // unlink head
                        lvl.head = maker.next_idx;
                        if (lvl.head != NIL) pool_[lvl.head].prev_idx = NIL; else lvl.tail = NIL;
                        // retire maker
                        handles_[maker.id] = NIL;
                        free_node(idx);
                    }
                }
                if (lvl.head == NIL) clear_level(asks_bits_, best_ask_, tick);
                else break; // still liquidity at 'tick' but buyer ran out or limit prevents moving on
            }

            if (remaining) {
                // time-in-force
                if ((in.flags & 0x2u) && remaining != in.qty) { // FOK but partial happened
                    // roll back is omitted for simplicity. For strict FOK you should prevent partial fill:
                    // enforce FOK by checking available qty before matching (pre-check by scanning ticks). Skipped here for hot path.
                }
                if ((in.flags & 0x1u)) return NIL; // IOC: do not rest
                return enqueue_resting(SIDE_BUY, in.price_tick, remaining);
            }
            return DONE_FILL;

        } else { // SELL
            while (remaining && best_bid_ != NO_PRICE && best_bid_ >= in.price_tick) {
                uint32_t tick = best_bid_;
                PriceLevel& lvl = bids_[tick];

                while (remaining && lvl.head != NIL) {
                    uint32_t idx = lvl.head;
                    OrderNode& maker = pool_[idx];

                    uint32_t trade = (remaining < maker.qty) ? remaining : maker.qty;
                    maker.qty -= trade;
                    remaining -= trade;
                    lvl.total_qty -= trade;

                    ++total_trades_;
                    total_volume_ += trade;

                    if (maker.qty == 0) {
                        lvl.head = maker.next_idx;
                        if (lvl.head != NIL) pool_[lvl.head].prev_idx = NIL; else lvl.tail = NIL;
                        handles_[maker.id] = NIL;
                        free_node(idx);
                    }
                }
                if (lvl.head == NIL) clear_level(bids_bits_, best_bid_, tick);
                else break;
            }

            if (remaining) {
                if ((in.flags & 0x1u)) return NIL; // IOC
                return enqueue_resting(SIDE_SELL, in.price_tick, remaining);
            }
            return DONE_FILL;
        }
    }

    // Cancel resting order by handle. Returns true if canceled.
    inline bool cancel(uint32_t handle) {
        if (unlikely(handle >= MAX_ORDERS)) return false;
        uint32_t idx = handles_[handle];
        if (idx == NIL) return false;

        OrderNode& n = pool_[idx];
        PriceLevel& lvl = (n.side == SIDE_BUY) ? bids_[n.price_tick] : asks_[n.price_tick];

        // unlink node from intrusive FIFO
        if (n.prev_idx != NIL) pool_[n.prev_idx].next_idx = n.next_idx; else lvl.head = n.next_idx;
        if (n.next_idx != NIL) pool_[n.next_idx].prev_idx = n.prev_idx; else lvl.tail = n.prev_idx;

        lvl.total_qty = (lvl.head == NIL) ? 0 : (lvl.total_qty - n.qty);

        if (lvl.head == NIL) {
            if (n.side == SIDE_BUY) clear_level(bids_bits_, best_bid_, n.price_tick);
            else                    clear_level(asks_bits_, best_ask_, n.price_tick);
        }

        handles_[n.id] = NIL;
        free_node(idx);
        return true;
    }

    // Replace: cancel old + add new (O(1) unlink, then normal add
    inline uint32_t replace(uint32_t handle, uint32_t new_tick, uint32_t new_qty) {
        if (unlikely(handle >= MAX_ORDERS || new_qty == 0 || new_tick >= MAX_TICKS)) return NIL;
        uint32_t idx = handles_[handle];
        if (idx == NIL) return NIL;
        const uint8_t side = pool_[idx].side;
        cancel(handle);
        OrderIn in{.client_id=0,.price_tick=new_tick,.qty=new_qty,.side=side,.flags=0,. _pad=0};
        return add_limit(in);
    }

    // Query best prices (NO_PRICE if empty)
    inline uint32_t best_bid() const { return best_bid_; }
    inline uint32_t best_ask() const { return best_ask_; }

    // Stats (not atomic since it calls from matching thread)
    inline uint64_t total_trades() const { return total_trades_; }
    inline uint64_t total_volume() const { return total_volume_; }

private:
    // Book state 
    std::array<PriceLevel, MAX_TICKS> bids_{};
    std::array<PriceLevel, MAX_TICKS> asks_{};
    std::array<uint64_t, WORDS> bids_bits_{}; // occupancy bitset by tick
    std::array<uint64_t, WORDS> asks_bits_{};
    uint32_t best_bid_{NO_PRICE};
    uint32_t best_ask_{NO_PRICE};

    //  Order pool & handle table
    std::array<OrderNode, MAX_ORDERS> pool_{};
    std::array<uint32_t, MAX_ORDERS> handles_{}; // handle -> pool index (NIL if not active)
    uint32_t free_head_{NIL}; // free-list head (pool index)
    uint32_t next_handle_{0}; // next candidate handle (wraps)

    // ---- Stats ----
    uint64_t total_trades_{0};
    uint64_t total_volume_{0};

    // ---- Pool helpers ----
    inline uint32_t alloc_node() {
        if (unlikely(free_head_ == NIL)) return NIL;
        uint32_t idx = free_head_;
        free_head_ = pool_[idx].next_idx;
        pool_[idx].next_idx = NIL;
        pool_[idx].prev_idx = NIL;
        return idx;
    }
    inline void free_node(uint32_t idx) {
        pool_[idx].next_idx = free_head_;
        free_head_ = idx;
    }

    // ---- Bitset helpers ----
    static inline void set_bit(std::array<uint64_t, WORDS>& bits, uint32_t tick) {
        bits[tick / WORD_BITS] |= (uint64_t(1) << (tick % WORD_BITS));
    }
    static inline void clear_bit(std::array<uint64_t, WORDS>& bits, uint32_t tick) {
        bits[tick / WORD_BITS] &= ~(uint64_t(1) << (tick % WORD_BITS));
    }
    static inline bool test_bit(const std::array<uint64_t, WORDS>& bits, uint32_t tick) {
        return (bits[tick / WORD_BITS] >> (tick % WORD_BITS)) & 1u;
    }

    // Find next ask >= 'from'
    inline uint32_t next_ask_from(uint32_t from) const {
        uint32_t w = from / WORD_BITS;
        uint32_t b = from % WORD_BITS;
        if (w >= WORDS) return NO_PRICE;

        uint64_t word = asks_bits_[w] & (~0ull << b);
        if (word) return w * WORD_BITS + std::countr_zero(word);

        for (++w; w < WORDS; ++w)
            if (asks_bits_[w]) return w * WORD_BITS + std::countr_zero(asks_bits_[w]);
        return NO_PRICE;
    }

    // Find prev bid <= 'from'
    inline uint32_t prev_bid_from(uint32_t from) const {
        uint32_t w = from / WORD_BITS;
        uint32_t b = from % WORD_BITS;
        if (w >= WORDS) return NO_PRICE;

        const uint64_t mask = (b == 63) ? ~0ull : ((uint64_t(1) << (b + 1)) - 1ull);
        uint64_t word = bids_bits_[w] & mask;
        if (word) return w * WORD_BITS + (63u - std::countl_zero(word));

        while (w--) {
            if (bids_bits_[w]) return w * WORD_BITS + (63u - std::countl_zero(bids_bits_[w]));
            if (w == 0) break;
        }
        return NO_PRICE;
    }

    // Maintain best pointers after a level empties
    inline void clear_level(std::array<uint64_t, WORDS>& bits, uint32_t& best, uint32_t emptied_tick) {
        clear_bit(bits, emptied_tick);
        if (&bits == &bids_bits_) {
            if (emptied_tick == best) best = (emptied_tick == 0) ? prev_bid_from(0) : prev_bid_from(emptied_tick - 1);
        } else {
            if (emptied_tick == best) best = next_ask_from(emptied_tick + 1);
        }
    }

    // Ensure best pointers after adding liquidity
    inline void ensure_best_after_add(uint8_t side, uint32_t tick) {
        if (side == SIDE_BUY) {
            if (best_bid_ == NO_PRICE || tick > best_bid_) best_bid_ = tick;
        } else {
            if (best_ask_ == NO_PRICE || tick < best_ask_) best_ask_ = tick;
        }
    }

    // Enqueue a resting order at tail of its level. returns handle
    inline uint32_t enqueue_resting(uint8_t side, uint32_t price_tick, uint32_t qty) {
        uint32_t idx = alloc_node();
        if (unlikely(idx == NIL)) return NIL;

        OrderNode& n = pool_[idx];
        n.price_tick = price_tick;
        n.qty        = qty;
        n.side       = side;

        // assign a free handle (O(1), wrap-safe)
        n.id = next_handle_;
        for (;;) {
            if (handles_[n.id] == NIL) { handles_[n.id] = idx; next_handle_ = (n.id + 1u) % MAX_ORDERS; break; }
            n.id = (n.id + 1u) % MAX_ORDERS;
        }

        PriceLevel& lvl = (side == SIDE_BUY) ? bids_[price_tick] : asks_[price_tick];

        // append to tail (FIFO price-time priority)
        n.prev_idx = lvl.tail;
        n.next_idx = NIL;
        if (lvl.tail != NIL) pool_[lvl.tail].next_idx = idx; else lvl.head = idx;
        lvl.tail = idx;
        lvl.total_qty += qty;

        // mark occupancy & adjust best
        if (side == SIDE_BUY) {
            if (!test_bit(bids_bits_, price_tick)) set_bit(bids_bits_, price_tick);
            ensure_best_after_add(SIDE_BUY, price_tick);
        } else {
            if (!test_bit(asks_bits_, price_tick)) set_bit(asks_bits_, price_tick);
            ensure_best_after_add(SIDE_SELL, price_tick);
        }
        return n.id;
    }
};
