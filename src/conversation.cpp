// src/conversation.cpp
// YOUR CODE.

#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::~Conversation() {
    delete[] data_;
}

Conversation::Conversation(const Conversation& other)
    : data_(other.size_ > 0 ? new Message[other.size_] : nullptr),
      size_(other.size_),
      capacity_(other.size_) {
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];  // Message's implicit copy (deep-copies content).
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;

    // Allocate the new buffer and copy into it BEFORE touching our own
    // state, so that if `new` throws (bad_alloc) or a Message copy throws,
    // *this is left completely untouched (strong exception guarantee) and
    // there's no risk of freeing our own data before we know the copy
    // succeeded.
    Message* new_data = other.size_ > 0 ? new Message[other.size_] : nullptr;
    for (std::size_t i = 0; i < other.size_; ++i) {
        new_data[i] = other.data_[i];
    }

    delete[] data_;
    data_ = new_data;
    size_ = other.size_;
    capacity_ = other.size_;
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;

    delete[] data_;

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::grow(std::size_t min_capacity) {
    std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * kGrowthFactor;
    if (new_capacity < min_capacity) new_capacity = min_capacity;

    Message* new_data = new Message[new_capacity];
    for (std::size_t i = 0; i < size_; ++i) {
        new_data[i] = std::move(data_[i]);  // steal each Message's string, no deep copy
    }
    delete[] data_;
    data_ = new_data;
    capacity_ = new_capacity;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        grow(size_ + 1);
    }
    data_[size_] = std::move(m);
    ++size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}
