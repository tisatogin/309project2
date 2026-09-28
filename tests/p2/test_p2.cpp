// tests/p2/test_p2.cpp
// YOUR CODE — 12+ assert-based tests covering Conversation, SentinelScanner,
// and correct use of the provided Harness/ModelClient.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------
// Tiny test-registration helper. Not part of the shipped code — just
// lets each TEST(name) { ... } block below run and report by name.
// ---------------------------------------------------------------------
int g_failures = 0;

#define TEST(name) void name()
#define RUN(name)                                    \
    do {                                              \
        std::cout << "[ RUN      ] " #name "\n";       \
        name();                                        \
        std::cout << "[       OK ] " #name "\n";       \
    } while (0)

// ---------------------------------------------------------------------
// Test-only I/O adapters for driving the provided Harness deterministically.
// ---------------------------------------------------------------------
class VectorInput : public InputSource {
public:
    explicit VectorInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (idx_ >= lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[idx_++];
    }
    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t idx_ = 0;
    bool eof_ = false;
};

class DiscardOutput : public OutputSink {
public:
    void write(std::string_view text) override { captured += text; }
    std::string captured;
};

// Writes `content` to `path`, for building throwaway .script / transcript
// fixture files that ScriptedModelClient / ReplayModelClient read from disk.
void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    assert(f.is_open() && "failed to create test fixture file");
    f << content;
}

// ===========================================================================
// Conversation tests
// ===========================================================================

TEST(EmptyConversationBounds) {
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool threw = false;
    try {
        conv.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw && "at() on an empty conversation must throw out_of_range");
}

TEST(SystemMessageStaysFirst) {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "hi"));
    conv.append(Message(Role::User, "another one"));
    conv.append(Message(Role::Assistant, "and another"));

    assert(conv.size() == 5);
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "Be concise.");
    // Nothing in Conversation ever removes or reorders elements, so the
    // system message (appended first by Harness's constructor) remains
    // pinned at index 0 for the conversation's entire lifetime.
    assert(conv.begin()->role() == Role::System);
}

TEST(CopyConstructorIsDeep) {
    Conversation original;
    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "hi there"));

    Conversation copy(original);

    // Different backing storage entirely -- not just a copied pointer.
    assert(copy.begin() != original.begin());
    assert(copy.size() == original.size());
    for (std::size_t i = 0; i < copy.size(); ++i) {
        assert(copy.at(i).content() == original.at(i).content());
        assert(copy.at(i).role() == original.at(i).role());
    }

    // Mutating one must not affect the other (proves it's not aliased).
    Conversation copy2(original);
    copy2.append(Message(Role::User, "extra"));
    assert(copy2.size() == original.size() + 1);
    assert(original.size() == 2);
}

TEST(CopyAssignmentIsDeepAndSelfSafe) {
    Conversation a;
    a.append(Message(Role::User, "a1"));
    Conversation b;
    b.append(Message(Role::User, "b1"));
    b.append(Message(Role::Assistant, "b2"));

    a = b;
    assert(a.begin() != b.begin());
    assert(a.size() == b.size());
    assert(a.at(1).content() == "b2");

    // Self-assignment must not corrupt or double-free.
    a = a;
    assert(a.size() == b.size());
    assert(a.at(0).content() == "b1");
}

TEST(MoveConstructorStealsAndEmptiesSource) {
    Conversation original;
    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "hi there"));

    const Message* original_ptr = original.begin();

    Conversation moved(std::move(original));

    // The new object owns the exact same buffer the source had.
    assert(moved.begin() == original_ptr);
    assert(moved.size() == 2);
    assert(moved.at(0).content() == "hello");

    // The moved-from source is left valid and empty -- safe to use or
    // destroy.
    assert(original.size() == 0);          // NOLINT(bugprone-use-after-move)
    assert(original.begin() == original.end());
}

TEST(MoveAssignmentStealsAndEmptiesSource) {
    Conversation original;
    original.append(Message(Role::User, "x"));
    const Message* original_ptr = original.begin();

    Conversation target;
    target.append(Message(Role::Assistant, "will be overwritten"));

    target = std::move(original);

    assert(target.begin() == original_ptr);
    assert(target.size() == 1);
    assert(target.at(0).content() == "x");
    assert(original.size() == 0);
    assert(original.begin() == original.end());
}

TEST(GrowthPreservesContentsAcrossReallocation) {
    Conversation conv;
    const int kCount = 200;  // forces several reallocations under doubling
    for (int i = 0; i < kCount; ++i) {
        Role role = (i % 2 == 0) ? Role::User : Role::Assistant;
        conv.append(Message(role, "msg-" + std::to_string(i)));
    }

    assert(conv.size() == static_cast<std::size_t>(kCount));
    for (int i = 0; i < kCount; ++i) {
        assert(conv.at(static_cast<std::size_t>(i)).content() == "msg-" + std::to_string(i));
        Role expected = (i % 2 == 0) ? Role::User : Role::Assistant;
        assert(conv.at(static_cast<std::size_t>(i)).role() == expected);
    }

    // Iteration order matches insertion order, oldest first.
    int i = 0;
    for (const Message* m = conv.begin(); m != conv.end(); ++m, ++i) {
        assert(m->content() == "msg-" + std::to_string(i));
    }
    assert(i == kCount);
}

// ===========================================================================
// SentinelScanner tests
// ===========================================================================

TEST(ScannerCleanTextNoSentinel) {
    SentinelScanner scanner("<|end_conversation|>");
    auto out = scanner.feed("Hello, how are you?");
    // With no sentinel candidate in progress, everything up to the last
    // (sentinel.size()-1) bytes is released immediately; the tiny tail is
    // held until flush() proves it's not a partial match.
    auto flushed = scanner.flush();
    assert(!out.sentinel_found);
    assert(!flushed.sentinel_found);
    assert(out.safe_text + flushed.safe_text == "Hello, how are you?");
}

TEST(ScannerWholeSentinelOneChunk) {
    SentinelScanner scanner("<|end_conversation|>");
    auto out = scanner.feed("Goodbye.<|end_conversation|>");
    assert(out.sentinel_found);
    assert(out.safe_text == "Goodbye.");
}

TEST(ScannerCatchesSentinelAtEveryBoundary) {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

TEST(ScannerCatchesSentinelOneByteAtATime) {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Hi there.  " + sentinel;
    SentinelScanner scanner(sentinel);
    std::string safe_so_far;
    bool found = false;
    for (char c : text) {
        auto out = scanner.feed(std::string_view(&c, 1));
        safe_so_far += out.safe_text;
        if (out.sentinel_found) {
            found = true;
            break;
        }
    }
    assert(found);
    assert(safe_so_far == "Hi there.  ");
}

TEST(ScannerDoesNotFalseAlarmOnPartialMatch) {
    SentinelScanner scanner("<|end_conversation|>");
    // Shares a long prefix with the sentinel ("<|end_") but diverges, so it
    // must never be reported as found and must eventually all be released.
    auto out = scanner.feed("well <|end_world|> not the sentinel");
    auto flushed = scanner.flush();
    assert(!out.sentinel_found);
    assert(!flushed.sentinel_found);
    assert(out.safe_text + flushed.safe_text == "well <|end_world|> not the sentinel");
}

TEST(ScannerPendingBufferStaysBounded) {
    const std::string sentinel = "<|end_conversation|>";
    const std::size_t limit = sentinel.size() - 1;
    SentinelScanner scanner(sentinel);

    // Adversarial: repeats a long common prefix of the sentinel without
    // ever completing it, one byte at a time, over a large stream --
    // exactly the shape that would blow up an O(N^2) "re-search everything"
    // implementation and the shape the 4MB ASan stress test exercises.
    const std::string adversarial_unit = "<|end_";
    std::string stream;
    while (stream.size() < 500000) stream += adversarial_unit;

    for (char c : stream) {
        scanner.feed(std::string_view(&c, 1));
        assert(scanner.pending_size() <= limit &&
               "pending_ must never hold more than sentinel.size()-1 bytes");
    }
    scanner.flush();
    assert(scanner.pending_size() == 0);
}

// ===========================================================================
// Harness / ModelClient integration tests (confirm correct wiring, not new
// logic -- Harness, ScriptedModelClient, ReplayModelClient are provided).
// ===========================================================================

TEST(HarnessStopsAtTurnLimit) {
    // A script with plenty of non-sentinel-terminated replies, and a turn
    // limit lower than the number of user lines supplied, so TurnLimit
    // (not UserExit or Sentinel) must be what stops the loop.
    const std::string script_path = "test_turnlimit.script";
    write_file(script_path,
               "role: assistant\n"
               "Reply one.\n"
               "---\n"
               "role: assistant\n"
               "Reply two.\n"
               "---\n"
               "role: assistant\n"
               "Reply three.\n");

    HarnessConfig cfg;
    cfg.max_turns = 2;
    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness harness(std::move(model), cfg);

    VectorInput in({"hi", "again", "and again"});
    DiscardOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(harness.conversation().size() == 4);  // 2 user + 2 assistant turns
    std::remove(script_path.c_str());
}

TEST(HarnessHaltsExactlyOnSentinel) {
    const std::string script_path = "test_sentinel.script";
    write_file(script_path,
               "chunk: 5\n"
               "role: assistant\n"
               "Fine, thanks!\n"
               "---\n"
               "chunk: 3\n"
               "role: assistant\n"
               "Goodbye.<|end_conversation|>\n");

    HarnessConfig cfg;
    cfg.max_turns = 20;
    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness harness(std::move(model), cfg);

    VectorInput in({"how are you", "bye"});
    DiscardOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::Sentinel);
    // The sentinel must never reach the printed output...
    assert(out.captured.find("<|end_conversation|>") == std::string::npos);
    // ...but must be preserved in the stored conversation, so a saved
    // transcript re-triggers the same stop when replayed.
    const Message& last = harness.conversation().at(harness.conversation().size() - 1);
    assert(last.content() == "Goodbye.<|end_conversation|>");
    std::remove(script_path.c_str());
}

TEST(TranscriptRoundTripThroughReplayClient) {
    // Build a conversation "by hand" and write it in Appendix A transcript
    // format, mirroring main.cpp's save_transcript(), then confirm
    // ReplayModelClient reproduces the assistant turns verbatim.
    const std::string transcript_path = "test_transcript.txt";
    write_file(transcript_path,
               "role: system\n"
               "Be concise.\n"
               "---\n"
               "role: user\n"
               "hello\n"
               "---\n"
               "role: assistant\n"
               "Hi! What can I do for you today?\n"
               "---\n"
               "role: user\n"
               "goodbye\n"
               "---\n"
               "role: assistant\n"
               "Goodbye.<|end_conversation|>");

    ReplayModelClient replay(transcript_path);
    assert(replay.system_message() == "Be concise.");

    Conversation dummy;  // ReplayModelClient::generate ignores conv content
    Message first = replay.generate(dummy);
    assert(first.role() == Role::Assistant);
    assert(first.content() == "Hi! What can I do for you today?");

    Message second = replay.generate(dummy);
    assert(second.content() == "Goodbye.<|end_conversation|>");

    // A third call must fail loudly (transcript exhausted) rather than
    // fabricate a reply.
    bool threw = false;
    try {
        replay.generate(dummy);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
    std::remove(transcript_path.c_str());
}

}  // namespace

int main() {
    RUN(EmptyConversationBounds);
    RUN(SystemMessageStaysFirst);
    RUN(CopyConstructorIsDeep);
    RUN(CopyAssignmentIsDeepAndSelfSafe);
    RUN(MoveConstructorStealsAndEmptiesSource);
    RUN(MoveAssignmentStealsAndEmptiesSource);
    RUN(GrowthPreservesContentsAcrossReallocation);

    RUN(ScannerCleanTextNoSentinel);
    RUN(ScannerWholeSentinelOneChunk);
    RUN(ScannerCatchesSentinelAtEveryBoundary);
    RUN(ScannerCatchesSentinelOneByteAtATime);
    RUN(ScannerDoesNotFalseAlarmOnPartialMatch);
    RUN(ScannerPendingBufferStaysBounded);

    RUN(HarnessStopsAtTurnLimit);
    RUN(HarnessHaltsExactlyOnSentinel);
    RUN(TranscriptRoundTripThroughReplayClient);

    std::cout << "\nAll " << 16 << " tests passed.\n";
    return g_failures == 0 ? 0 : 1;
}
