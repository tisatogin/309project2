// include/core/message.h
// YOUR CODE — Message is a small value type: a role + string content.
// Header-only (no message.cpp needed) since every member is trivial.

#pragma once
#include <string>
#include <utility>

enum class Role { System, User, Assistant };

class Message {
public:
    // Default-constructs an empty System message with empty content.
    // Needed so Conversation can allocate raw array slots (via `new
    // Message[n]`) before append() fills them in — a raw array of a
    // class type requires that type to be default-constructible.
    Message() : role_(Role::System), content_() {}

    Message(Role role, std::string content)
        : role_(role), content_(std::move(content)) {}

    Role role() const noexcept { return role_; }
    const std::string& content() const noexcept { return content_; }

private:
    Role role_;
    std::string content_;
};
