// include/core/sentinel_scanner.h
// YOUR CODE — incremental sentinel detector over an arbitrarily-chunked
// character stream, using O(sentinel size) memory.

#pragma once
#include <cstddef>
#include <string>
#include <string_view>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel) : sentinel_(std::move(sentinel)) {}

    struct Out {
        std::string safe_text;
        bool sentinel_found;
    };

    // Feed the next chunk. Returns text guaranteed NOT to be part of
    // the sentinel (safe to print immediately) and whether the
    // sentinel has now been fully seen. Once sentinel_found is true,
    // further feed() calls are a no-op (returns {"", true}) — callers
    // (the Harness) are expected to stop feeding at that point anyway.
    Out feed(std::string_view chunk);

    // Call once, after the stream ends, to release any text still
    // being held back (guaranteed not to contain the sentinel, since
    // if it had been seen, feed() would already have reported it).
    Out flush();

    // Test-only accessor: exposes how many bytes are currently held back
    // in pending_. Not used by production code (harness.cpp never calls
    // this) — it exists so the test suite can assert the bounded-memory
    // guarantee (pending_.size() <= sentinel_.size() - 1) directly,
    // rather than only inferring it indirectly.
    std::size_t pending_size() const noexcept { return pending_.size(); }

private:
    std::string sentinel_;
    std::string pending_;   // holds back at most sentinel_.size() - 1
                             // trailing characters that could still
                             // become the start of the sentinel
    bool found_ = false;
};
