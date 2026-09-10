#pragma once

#include <cstdint>
#include <vector>

namespace ob {
  using Idx          = uint32_t;
  using Qty          = int64_t;
  constexpr Idx NONE = UINT32_MAX;
  enum class Side : uint8_t { Buy, Sell };

  /// Reports a single fill produced while matching.
  ///
  /// Fired once per fill, so possibly zero times for one order. The book is in a
  /// half-updated state during the call: the maker's quantity has already been
  /// reduced and the taker has not been rested yet.
  ///
  /// @param ctx         Opaque pointer the caller passed to add_limit/add_market,
  ///                    forwarded untouched. Cast it back to your own type.
  /// @param maker_slot  Pool slot of the resting order that was hit. Look it up in
  ///                    your own side table to recover the order id and account.
  /// @param level       Price level index where the trade happened. Always the
  ///                    maker's level, never the taker's.
  /// @param qty         Quantity filled by this fill alone, not the running total.
  ///
  /// @warning Never call back into the book (add_limit, add_market, cancel) from here.
  typedef void (*OnFill)(void* ctx, Idx maker_slot, Idx level, Qty qty);

  struct OrderNode {
    Qty qty  = 0;
    Idx prev = NONE;
    Idx next = NONE;
  };

  struct Level {
    Qty total      = 0;
    Idx head       = NONE;
    Idx tail       = NONE;
    uint32_t count = 0;
  };

  class OrderBook {
    std::vector<OrderNode> pool_;
    std::vector<Level> levels_;
    std::vector<uint64_t> occupied_;
    Idx free_head_ = NONE;
    Idx best_bid_  = NONE;
    Idx best_ask_  = NONE;

  public:
    OrderBook(Idx level_capacity, Idx pool_capacity);

    /// Adds a limit order: matches while crossing, rests the remainder.
    /// @return       Pool slot of the rested remainder, or NONE when the order was
    ///               fully filled or the pool is exhausted.
    Idx add_limit(Side side, Idx level, Qty qty, OnFill onFill, void* ctx);

    /// Adds a market order: matches as far as the opposite side allows, drops the rest.
    /// @return      Quantity left unfilled and discarded; 0 means fully filled.
    ///              A market order never rests, so there is no slot to return.
    Qty add_market(Side side, Qty qty, OnFill onFill, void* ctx);

    bool cancel(Idx slot, Idx level);

  private:
    Idx alloc_slot();
    void free_slot(Idx slot);

    /// Find the right-most smallest bit
    Idx find_up(Idx from);

    /// Find the left-most biggest bit
    Idx find_down(Idx from);

    void clear_bit(Idx level);
  };
}  // namespace ob
