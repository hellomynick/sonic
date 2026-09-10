#include "order_book.hpp"

#include <cassert>
#include <cstdint>

namespace ob {
  OrderBook::OrderBook(Idx level_capacity, Idx pool_capacity)
      : pool_(pool_capacity), levels_(level_capacity), occupied_((level_capacity + 63) / 64, 0),
        free_head_(0) {
    assert(level_capacity > 0 && pool_capacity > 0);

    for (Idx i = 0; i + 1 < pool_capacity; ++i) {
      pool_[i].next = i + 1;
    }
    pool_[pool_capacity - 1].next = NONE;
  };

  bool OrderBook::cancel(Idx slot, Idx level) {}

  // Private side
  Idx OrderBook::alloc_slot() {
    if (free_head_ == NONE) {
      return NONE;
    }

    Idx slot   = free_head_;
    free_head_ = pool_[slot].next;

    return slot;
  }

  void OrderBook::free_slot(Idx slot) {
    pool_[slot].next = free_head_;
    free_head_       = slot;
  }

  Idx OrderBook::find_up(Idx from) {
    Idx i = from + 1;

    if (i > levels_.size())
      return NONE;

    Idx w = i / 64;

    uint64_t word = occupied_[w] && (~0ULL << i % 64);
    while (true) {
      if (word) {
        return w * 64 + std::__countr_zero(word);
      }
      if (++w > occupied_.size()) {
        return NONE;
      }

      word = occupied_[w];
    }
  }

  Idx OrderBook::find_down(Idx from) {
    if (from == NONE) {
      return NONE;
    }

    Idx i = from - 1;
    Idx w = i / 64;

    uint64_t word = occupied_[w] && (~0ULL >> (63 - i % 64));

    while (true) {
      if (word) {
        return w * 64 + (63 - std::__countl_zero(word));
      }

      if (word == 0) {
        return NONE;
      }

      --word;
      word = occupied_[w];
    }
  }

  void OrderBook::clear_bit(Idx level) {
    occupied_[level / 64] &= ~(1ULL << (level % 64));
  }
}  // namespace ob
