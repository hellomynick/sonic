#include <cstdint>
#include <ratio>
#include <vector>

using Idx = uint32_t;

struct OrderNode {
    int64_t qty = 0;
    Idx prev = UINT32_MAX;
    Idx next = UINT32_MAX;
};

struct Level {
    int64_t qty = 0;
    Idx head = UINT32_MAX;
    Idx tail = UINT32_MAX;
    uint32_t count = 0;
};

class OrderBook {
 std::vector<OrderNode> pool_;
 std::vector<Level> bids_, asks_;
 std::vector<uint64_t> occupied_;

 public:
     OrderBook();
     void add_limit();
     void add_market();
     void cancel();
};
