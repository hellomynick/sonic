#include <cstdint>
#include <vector>

using Idx          = uint32_t;
constexpr Idx NONE = UINT32_MAX;

struct OrderNode {
  int64_t qty = 0;
  Idx prev    = NONE;
  Idx next    = NONE;
};

struct Level {
  int64_t total  = 0;
  Idx head       = NONE;
  Idx tail       = NONE;
  uint32_t count = 0;
};

class OrderBook {
  std::vector<OrderNode> pool_;
  std::vector<Level> levels_;
  std::vector<uint64_t> occupied_;
  Idx free_head_    = NONE;
  int64_t best_bid_ = NONE;
  int64_t best_ask_ = NONE;

public:
  OrderBook(Idx level_capacity, Idx pool_capacity);
  Idx add_limit();
  int64_t add_market();
  bool cancel();
};
