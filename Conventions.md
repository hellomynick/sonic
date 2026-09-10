# Conventions

Project-wide conventions. Each decision includes the reason, so future-you knows why.
Update this file when a decision changes — never let the code and this file drift apart.

---

## 1. Languages and build

| Part | Language | Build |
|---|---|---|
| `engine/`, `simulator/` | C++20 written in a C style | Meson |
| `order-api/`, `ledger/`, `gateway/`, `analytics/` | Rust | Cargo workspace |
| `web/` | TypeScript | Vite |
| `schema/` | FlatBuffers `.fbs` | `flatc`, invoked by Meson and `build.rs` |

The two build systems do not link against each other. A root `justfile` drives both.
Services talk over ZeroMQ + FlatBuffers. No FFI.

## 2. C++ style

Write C++ in a C style: prefer simple and predictable over abstract.

**Use from C++**
- `std::vector` instead of `malloc/free`
- `struct` with default member initializers (`ob_idx head = OB_NONE;`)
- `enum class` instead of `#define`
- `constexpr` / `static const` for constants
- `static_assert`, `<bit>` (`std::countr_zero`), `std::span`
- `class` only to group data with functions and hide private state

**Avoid**
- Inheritance, `virtual`, runtime polymorphism
- Exceptions (return an error code or `OB_NONE`)
- `std::function`, `shared_ptr`, `std::string` on the hot path
- Templates unless there is a measured reason (callbacks use function pointers)
- Pointers into containers — use **indices**

## 3. Naming

| Types      | PascalCase, no prefix | `Side`, `Level`, `OrderNode`, `OrderBook` |
| Aliases    | PascalCase            | `Idx` |
| Constants  | UPPER_CASE            | `NONE` |
| Functions  | snake_case            | `add_limit` |
| Members    | trailing `_`          | `pool_`, `best_bid_` |

All engine core code lives in `namespace ob`.

## 4. Numeric types

| Used for | Type | Reason |
|---|---|---|
| Quantities, totals, prices (in ticks), money | `int64_t` | large enough for any lot size; signed so `assert(qty >= 0)` catches underflow; differences can be negative |
| Array indices: slot, level | `ob_idx` = `uint32_t` | a few million is plenty; matches `size_t` closely enough to avoid casts |
| "No index" sentinel | `OB_NONE = UINT32_MAX` | named, instead of repeating `UINT32_MAX` |
| Bitmaps, masks | `uint64_t` | shifting into the sign bit is UB on signed types |
| Small flags, enums | `uint8_t` | |

**No floating point anywhere.** Prices are tick counts, quantities are lot counts, both integers.
Conversion to decimal happens only at the edges (order-api, web).

## 5. Data structs

- Order fields **largest first** so there is no padding. Verify with `static_assert(sizeof(X) == N)`.
- Keep them POD: no complex constructors, `static_assert(std::is_trivially_copyable_v<X>)`.
- Never store what can be derived: `ObLevel` has no `price` because price is the array index.
- Split hot from cold: hot-path fields together, metadata (id, account, timestamp) in a separate array.

## 6. Trust boundary

```
user (untrusted)
  → order-api        : auth, validation, rate limiting
  → engine service   : verify FlatBuffers buffer
  → SymbolEngine     : id → {slot, level, gen}, generation check, STP, tick/lot rules
  → OrderBook        : receives already-validated numbers, asserts only
```

Validate at the **edge**, assert on the **inside**. Do not make the innermost layer defend
against input it never receives directly.

- Debug: dense `assert`s, `check_invariants()` after every operation in tests.
- Release: asserts compile away, the hot path does not re-check.

## 7. Error handling

- No exceptions. Return an error code or a sentinel value.
- `add_limit` → the slot that got rested, or `OB_NONE` (fully matched / pool exhausted).
- `add_market` → the quantity that could not be filled (0 means fully filled).
- `cancel` → `bool`.
- Detailed business reject codes (`InvalidTick`, `SelfTrade`, …) belong to `SymbolEngine`, not the book.

## 8. Callbacks

```cpp
typedef void (*ObOnFill)(void* ctx, ob_idx maker_slot, ob_idx level, int64_t qty);
```

- `ctx` is a **pointer to the caller's data**. The book only passes it through: it never stores it
  and never uses it after the call returns.
- Do not smuggle a function pointer through `ctx`; if a second behaviour is needed, add a second
  callback parameter.
- The callback fires **during** matching, while the book is in a half-updated state:
  **never call back into the book** (`add_limit`, `cancel`) from inside a callback.
- It fires once per fill, so possibly zero times.

## 9. Invariants maintained by hand

Anything that is derived data (a cache) must be updated together with its source,
and `check_invariants()` must verify it:

- `ObLevel::total` equals the sum of `qty` over the nodes in that level
- `ObLevel::count` equals the number of nodes in that level
- bit `i` in `occupied_` is set ⇔ `levels_[i].count > 0`
- `best_bid_` is the highest occupied buy level; `best_ask_` the lowest occupied sell level
- `best_bid_ < best_ask_` whenever both exist
- `prev`/`next` are symmetric; the free list has no cycles and contains no live slot

## 10. Performance

- No allocation after the constructor on the hot path. The pool is allocated once and never grows.
- Pool exhausted or level out of range → reject; never expand at runtime.
- Every optimisation needs before/after numbers recorded in `BENCHMARK.md`. No optimising on a hunch.
- Measure with Google Benchmark plus an HDR histogram (p50/p99/p99.9), never the mean alone.

## 11. Formatting

- `.clang-format` at the repo root, `IndentWidth: 2`, `ColumnLimit: 100`. clangd reads it automatically.
  After editing it, restart the language server.
- `.editorconfig` for editors that do not use clangd.
- Rust: `rustfmt.toml`, `cargo fmt`.
- CI runs `clang-format --dry-run --Werror` and `cargo fmt --check`.

## 12. Open decisions

Settle these when the work reaches them, then record the outcome here:

- [ ] Should `ObOrderNode` store its `level`? Current: **no** — the caller passes it to
      `cancel(level, slot)`, and a Debug-only `dbg_level_` array asserts it. Revisit if the
      8 bytes turn out to be worth the extra safety.
- [ ] One shared level array or separate `bids_`/`asks_`? Current: **one shared array**, one bitmap.
- [ ] Callbacks: function pointer or template? Current: **function pointer**; revisit at #20 if the
      benchmark says the indirect call matters.
- [ ] Do acks share the PUB socket, or get a dedicated socket to order-api?
- [ ] What price does the ledger reserve against for a market buy?
- [ ] Empty candles: skip them, or emit `open = close = previous close`?
- [ ] Who assigns order ids, order-api or the engine? Current: **order-api**.
