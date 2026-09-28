// src/sentinel_scanner.cpp
// YOUR CODE.

#include "core/sentinel_scanner.h"

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (found_) {
        // Already reported; the harness stops calling us after this, but
        // stay well-defined regardless.
        return {"", true};
    }

    // Treat pending_ + chunk as the text to search, per the spec's
    // algorithm. pending_ is at most sentinel_.size() - 1 bytes, and chunk
    // is one caller-supplied piece, so this concatenation (and the search
    // over it) costs O(pending_.size() + chunk.size()) — not proportional
    // to everything ever seen.
    std::string combined;
    combined.reserve(pending_.size() + chunk.size());
    combined.append(pending_);
    combined.append(chunk.data(), chunk.size());

    std::size_t pos = combined.find(sentinel_);
    if (pos != std::string::npos) {
        found_ = true;
        pending_.clear();
        return {combined.substr(0, pos), true};
    }

    // No match yet: hold back the last (sentinel_.size() - 1) characters,
    // since they could be an in-progress prefix of the sentinel that will
    // be completed by a future chunk. Everything before that is guaranteed
    // not to be part of any sentinel occurrence and is safe to emit now.
    const std::size_t hold_back = sentinel_.empty() ? 0 : sentinel_.size() - 1;

    if (combined.size() <= hold_back) {
        pending_ = std::move(combined);
        return {"", false};
    }

    const std::size_t safe_len = combined.size() - hold_back;
    Out out;
    out.safe_text = combined.substr(0, safe_len);
    out.sentinel_found = false;
    pending_ = combined.substr(safe_len);
    return out;
}

SentinelScanner::Out SentinelScanner::flush() {
    // If the sentinel had been found, feed() already reported it and
    // cleared pending_; flush() is only meaningful for the "stream ended
    // cleanly, release whatever we were holding back" case.
    Out out{pending_, found_};
    pending_.clear();
    return out;
}
