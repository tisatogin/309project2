// include/core/conversation.h
// YOUR CODE — a hand-rolled growable array of Message. std::vector is
// forbidden; this is the only place in the codebase that uses raw new/delete.

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // Empty conversation: size() == 0, no allocation yet.
    Conversation() noexcept = default;

    // Releases all owned Message storage. No effect if already empty
    // (e.g. moved-from) since delete[] on nullptr is a no-op.
    ~Conversation();

    // Deep copy: allocates its own buffer and copies every Message.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer — no per-element copying. Afterward, other
    // is left valid and empty.
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Appends m, growing the backing array if needed. Amortized O(1) —
    // see docs/design-log-p2.md for the growth-factor proof.
    void append(Message m);

    // Number of messages currently stored.
    std::size_t size() const noexcept { return size_; }

    // Bounds-checked access. Throws std::out_of_range on i >= size().
    const Message& at(std::size_t i) const;

    // Range-for iteration, oldest message first.
    const Message* begin() const noexcept { return data_; }
    const Message* end() const noexcept { return data_ + size_; }

private:
    // Growth factor for reallocation. See design log for the amortized
    // O(1) proof; any factor > 1 works, 2 is a common, simple choice.
    static constexpr std::size_t kGrowthFactor = 2;

    // Grows the backing array to at least `min_capacity`, moving existing
    // elements into the new buffer and freeing the old one.
    void grow(std::size_t min_capacity);

    Message* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
