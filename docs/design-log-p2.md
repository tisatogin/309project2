# Design Log — Project 2

## Growth factor and amortized cost

`Conversation` grows by doubling: `new_capacity = capacity_ == 0 ? 1 : capacity_ * 2`
(`kGrowthFactor = 2` in `conversation.h`). `append` only reallocates when
`size_ == capacity_`, and each reallocation copies the `size_` existing
elements into the new buffer.

**Proof of amortized O(1):** Over `n` appends from empty, reallocations
happen exactly at capacities `2^0, 2^1, ..., 2^k` where `2^k` is the
largest power of two `<= n`. Total elements copied across all
reallocations is the geometric series `1 + 2 + 4 + ... + 2^k = 2^(k+1) - 1
< 2n`. So total copying work over `n` appends is `O(n)`, meaning the
*average* (amortized) cost per `append` is `O(1)`, even though any single
`append` that triggers a reallocation costs `O(size_)` worst-case.
Equivalently, by the accounting method: charge each `append` 3 credits.
1 pays for inserting the new element; the other 2 accumulate. When a
reallocation at size `k` occurs, the `k` elements added since the previous
reallocation (at size `k/2`) have banked `2 * (k/2) = k` credits — exactly
enough to pay for copying all `k` existing elements. The credit balance
never goes negative, so amortized cost per operation is `O(1)`.

This only works because the factor is a constant `> 1`. A fixed increment
(e.g. `capacity_ + 1`) instead reallocates on *every* `append`, each copying
`size_` elements, for `1 + 2 + ... + n = O(n^2)` total — the failure mode
doubling avoids.

## Rule of Five evidence

- **Destructor:** `delete[] data_` — safe even on a moved-from object since
  `data_` is `nullptr` there and `delete[] nullptr` is a no-op.
- **Copy ctor/assignment:** allocate a *new* `Message[]` buffer sized to the
  source and copy each `Message` element-by-element (using `Message`'s
  implicit copy, which deep-copies its `std::string`). `test_p2.cpp`'s
  `CopyConstructorIsDeep` asserts `copy.begin() != original.begin()` —
  different addresses, not an aliased pointer — then mutates one copy and
  confirms the other is unaffected. Copy assignment builds the replacement
  buffer *before* freeing the old one, so a self-assignment (`a = a`,
  tested explicitly) or a mid-copy exception never leaves `*this` in a
  freed-then-used state.
- **Move ctor/assignment:** steal `data_`/`size_`/`capacity_` directly and
  zero out the source's members, leaving it in a valid empty state
  (`size() == 0`, `begin() == end()`, safe to destroy or reuse).
  `MoveConstructorStealsAndEmptiesSource` captures the source's `begin()`
  pointer *before* the move and asserts the destination's `begin()` is the
  identical address — proof of pointer-stealing, not a hidden deep copy —
  then asserts the source is empty.
- Building under `-fsanitize=address,undefined` and running the full
  16-case suite (which repeatedly copies, moves, self-assigns, and lets
  `Conversation`s go out of scope in loops) reports zero leaks and no
  undefined behavior — the strongest evidence against the two pitfalls in
  spec §4 (ambiguous ownership, shallow-copy double-free).

## Sentinel scanner: bounded pending_ proof

On each `feed(chunk)`, `combined = pending_ + chunk` is searched for the
sentinel. If not found, everything in `combined` except the last
`sentinel_.size() - 1` bytes is released as safe, and exactly those
trailing `sentinel_.size() - 1` bytes (or fewer, if `combined` itself is
shorter) become the new `pending_`.

**Invariant:** after every `feed()` call, `pending_.size() <=
sentinel_.size() - 1`. This holds by construction: the code path either (a)
sets `pending_ = combined` only when `combined.size() <= hold_back`, where
`hold_back = sentinel_.size() - 1`, or (b) sets `pending_ =
combined.substr(safe_len)`, which has length exactly `combined.size() -
safe_len = hold_back`. Both branches produce `pending_.size() <= hold_back`.
Since this holds after every call regardless of the previous `pending_`
(the invariant re-establishes itself each time rather than compounding),
it holds for all `n`, by induction on the number of `feed()` calls.

This is correct, not just bounded: any *in-progress* occurrence of the
sentinel spanning chunk boundaries must, while incomplete, be a proper
suffix of what's been seen so far, and a proper suffix of the sentinel has
length `<= sentinel_.size() - 1`. Holding back exactly that many trailing
bytes is always enough to avoid emitting a partial sentinel, and never more
than necessary. `ScannerPendingBufferStaysBounded` stress-feeds 500,000
bytes of an adversarial repeating near-miss (`"<|end_"` repeated, one byte
at a time) and asserts the invariant after every `feed()` call.

This bound is also what makes the algorithm O(1) *space* and, per-chunk,
O(chunk size) time rather than O(N) space / O(N^2) time — searching a
freshly concatenated "everything ever received" string on every chunk
would re-scan already-cleared text repeatedly.

## What I would change differently

I'd replace the `std::string::find` re-search of `pending_ + chunk` with
Knuth-Morris-Pratt, keeping only the KMP failure-function state across
calls instead of re-scanning the small `combined` buffer each time. The
current approach is already O(1) space and linear overall (each `feed`
only searches `pending_.size() + chunk.size() <= sentinel_.size() - 1 +
chunk.size()` bytes, never the whole stream), but KMP would drop even that
small constant-factor re-scan — worth it if `SentinelScanner` were reused
for longer or multiple concurrent sentinels.
