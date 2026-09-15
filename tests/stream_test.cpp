#include "stream.h"
#include <gtest/gtest.h>
#include <array>
#include <vector>

TEST(Configuration, OnlyVariantThreeSettingsAreAccepted)
{
    EXPECT_TRUE(valid_config(3, 1));
    EXPECT_TRUE(valid_config(123, 2));
    EXPECT_TRUE(valid_config(65535, 2));
    EXPECT_FALSE(valid_config(0, 1));
    EXPECT_FALSE(valid_config(65536, 1));
    EXPECT_FALSE(valid_config(3, 0));
    EXPECT_FALSE(valid_config(3, 15));
    EXPECT_FALSE(valid_config(3, 3));
}

TEST(Utf8, KnownWireBytes)
{
    const std::vector<std::pair<uint32_t, std::vector<unsigned char>>> cases = {
        {'A', {0x41}}, {'\n', {0x0a}}, {0x42f, {0xd0, 0xaf}},
        {0x20ac, {0xe2, 0x82, 0xac}}, {0x1f642, {0xf0, 0x9f, 0x99, 0x82}}
    };
    for (const auto &entry : cases) {
        unsigned char bytes[4]{};
        auto size = utf8_encode(entry.first, bytes);
        EXPECT_EQ(std::vector<unsigned char>(bytes, bytes + size), entry.second);
    }
}

TEST(Utf8, EveryUnicodeScalarRoundTripsOneByteAtATime)
{
    for (uint32_t scalar = 0; scalar <= 0x10ffff; ++scalar) {
        if (scalar >= 0xd800 && scalar <= 0xdfff) continue;
        unsigned char bytes[4];
        auto size = utf8_encode(scalar, bytes);
        decoder_t decoder{};
        uint32_t output = 0;
        ASSERT_GE(size, 1u);
        for (size_t i = 0; i < size; ++i)
            ASSERT_EQ(utf8_feed(&decoder, bytes[i], &output), i + 1 == size ? 1 : 0);
        ASSERT_EQ(output, scalar);
    }
}

TEST(Utf8, MalformedSequencesAreRejected)
{
    const std::vector<std::vector<unsigned char>> invalid = {
        {0x80}, {0xc0}, {0xc1}, {0xff}, {0xf5}, {0xe0, 0x80, 0xaf},
        {0xed, 0xa0, 0x80}, {0xf4, 0x90, 0x80, 0x80}, {0xc2, 0x41},
        {0xf0, 0x80, 0x80, 0xaf}
    };
    for (const auto &sequence : invalid) {
        decoder_t decoder{};
        uint32_t scalar = 0;
        int result = 0;
        for (auto byte : sequence) { result = utf8_feed(&decoder, byte, &scalar); if (result < 0) break; }
        EXPECT_EQ(result, -1);
        EXPECT_EQ(utf8_feed(&decoder, 'A', &scalar), 1);
        EXPECT_EQ(scalar, static_cast<uint32_t>('A'));
    }
}

TEST(Utf8, IncompleteCharacterWaitsWithoutInventingOutput)
{
    decoder_t decoder{};
    uint32_t scalar = 123;
    EXPECT_EQ(utf8_feed(&decoder, 0xf0, &scalar), 0);
    EXPECT_EQ(utf8_feed(&decoder, 0x9f, &scalar), 0);
    EXPECT_EQ(utf8_feed(&decoder, 0x99, &scalar), 0);
    EXPECT_EQ(scalar, 123u);
    EXPECT_EQ(utf8_feed(&decoder, 0x82, &scalar), 1);
    EXPECT_EQ(scalar, 0x1f642u);
}

TEST(Text, PrintableCharactersAndEnterAreAccepted)
{
    for (uint32_t scalar = 32; scalar < 127; ++scalar) EXPECT_TRUE(is_text(scalar));
    for (auto scalar : {0x42fu, 0x20acu, 0x1f642u, 0x301u, 10u}) EXPECT_TRUE(is_text(scalar));
    for (auto scalar : {0u, 8u, 9u, 13u, 27u, 127u, 0x85u, 0xd800u, 0x110000u}) EXPECT_FALSE(is_text(scalar));
    unsigned char bytes[4];
    EXPECT_EQ(utf8_encode(0xdfff, bytes), 0u);
    EXPECT_EQ(utf8_encode(0x110000, bytes), 0u);
}

class StreamTransport : public ::testing::Test {
protected:
    std::vector<unsigned char> wire;
    size_t fail_after = SIZE_MAX;
    uint64_t completed = 0;
    static int write(void *context, unsigned char byte) {
        auto self = static_cast<StreamTransport *>(context);
        if (self->wire.size() == self->fail_after) return 0;
        self->wire.push_back(byte);
        return 1;
    }
    int send(uint32_t scalar) { return write_character(scalar, write, this, &completed); }
};

TEST_F(StreamTransport, SendsImmediatelyWithoutMessageFraming)
{
    ASSERT_TRUE(send('A'));
    EXPECT_EQ(wire, (std::vector<unsigned char>{'A'})); // No Enter needed.
    ASSERT_TRUE(send(0x42f));
    ASSERT_TRUE(send('\n'));
    EXPECT_EQ(wire, (std::vector<unsigned char>{0x41, 0xd0, 0xaf, 0x0a}));
    EXPECT_EQ(completed, 3u);
}

TEST_F(StreamTransport, PartialCharacterDoesNotIncreaseCounterOrRetry)
{
    ASSERT_TRUE(send('A'));
    fail_after = 2;
    EXPECT_FALSE(send(0x1f642));
    EXPECT_EQ(wire, (std::vector<unsigned char>{'A', 0xf0}));
    EXPECT_EQ(completed, 1u);
}

TEST_F(StreamTransport, InvalidInputNeverTouchesTransport)
{
    EXPECT_FALSE(send(0xd800));
    EXPECT_FALSE(send(0));
    EXPECT_TRUE(wire.empty());
    EXPECT_EQ(completed, 0u);
}

TEST_F(StreamTransport, RepeatedMessagesRemainExact)
{
    const std::vector<uint32_t> message{'H', 'i', ' ', 0x41f, 0x440, 0x438, 0x432, 0x435, 0x442, 0x1f642, '\n'};
    for (int cycle = 0; cycle < 100; ++cycle) for (auto scalar : message) ASSERT_TRUE(send(scalar));
    decoder_t decoder{};
    std::vector<uint32_t> received;
    for (auto byte : wire) {
        uint32_t scalar;
        int result = utf8_feed(&decoder, byte, &scalar);
        ASSERT_GE(result, 0);
        if (result) received.push_back(scalar);
    }
    ASSERT_EQ(received.size(), message.size() * 100);
    EXPECT_EQ(completed, received.size());
    for (size_t i = 0; i < received.size(); ++i) EXPECT_EQ(received[i], message[i % message.size()]);
}

class CharacterQueue : public ::testing::Test {
protected:
    queue_t queue{};
};

TEST_F(CharacterQueue, FullQueueRejectsInputWithoutOverwritingOldCharacters)
{
    for (unsigned i = 0; i < QUEUE_CAPACITY; ++i) ASSERT_TRUE(queue_push(&queue, 32 + i % 95));
    EXPECT_FALSE(queue_push(&queue, 'X'));
    for (unsigned i = 0; i < QUEUE_CAPACITY; ++i) {
        uint32_t scalar;
        ASSERT_TRUE(queue_pop(&queue, &scalar));
        ASSERT_EQ(scalar, 32 + i % 95);
    }
    uint32_t scalar;
    EXPECT_FALSE(queue_pop(&queue, &scalar));
}

TEST_F(CharacterQueue, WrapsAcrossManySendReceiveCycles)
{
    for (unsigned i = 0; i < QUEUE_CAPACITY * 3; ++i) {
        uint32_t scalar;
        ASSERT_TRUE(queue_push(&queue, 32 + i % 95));
        ASSERT_TRUE(queue_pop(&queue, &scalar));
        ASSERT_EQ(scalar, 32 + i % 95);
    }
    EXPECT_EQ(queue.count, 0u);
    EXPECT_FALSE(queue_push(&queue, '\b'));
}
