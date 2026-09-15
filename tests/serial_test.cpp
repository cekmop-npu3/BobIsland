#include "backend.h"
#include <windows.h>
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <vector>

static int discard(void *, uint32_t) { return 1; }

TEST(WindowsBackend, InvalidConfigurationFailsBeforeOpeningHardware)
{
    unsigned long error = 0;
    EXPECT_EQ(serial_open(0, 1, discard, nullptr, &error), nullptr);
    EXPECT_EQ(error, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(serial_open(3, 15, discard, nullptr, &error), nullptr);
    EXPECT_EQ(error, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(serial_open(3, 1, nullptr, nullptr, &error), nullptr);
    EXPECT_EQ(error, ERROR_INVALID_PARAMETER);
    EXPECT_FALSE(serial_send(nullptr, 'A', &error));
    EXPECT_EQ(error, ERROR_INVALID_PARAMETER);
    EXPECT_FALSE(serial_get_status(nullptr).running);
    serial_close(nullptr);
}

TEST(WindowsBackend, EnumerationDoesNotRequireConnectedHardware)
{
    unsigned long error = 0;
    auto count = serial_ports(nullptr, 0, &error);
    ASSERT_NE(count, SIZE_MAX);
    EXPECT_EQ(error, ERROR_SUCCESS);
    std::vector<unsigned> ports(count);
    ASSERT_EQ(serial_ports(ports.data(), ports.size(), &error), count);
    for (size_t i = 0; i < count; ++i) {
        EXPECT_GE(ports[i], 1u);
        if (i) { EXPECT_LT(ports[i - 1], ports[i]); }
    }
}

class SerialPair : public ::testing::Test {
protected:
    struct Inbox {
        std::mutex mutex;
        std::condition_variable changed;
        std::vector<uint32_t> text;
        static int receive(void *context, uint32_t scalar) {
            auto &self = *static_cast<Inbox *>(context);
            std::lock_guard<std::mutex> guard(self.mutex);
            self.text.push_back(scalar);
            self.changed.notify_all();
            return 1;
        }
        bool wait(size_t size) {
            std::unique_lock<std::mutex> guard(mutex);
            return changed.wait_for(guard, std::chrono::seconds(10), [&]{ return text.size() >= size; });
        }
    } a_inbox, b_inbox;
    serial_t *a = nullptr, *b = nullptr;
    unsigned a_port = 0, b_port = 0;
    void SetUp() override {
        const char *a_value = std::getenv("TEST_PORT_A");
        const char *b_value = std::getenv("TEST_PORT_B");
        if (!a_value || !b_value) GTEST_SKIP() << "Set TEST_PORT_A and TEST_PORT_B to a connected COM pair (numbers only).";
        a_port = static_cast<unsigned>(std::strtoul(a_value, nullptr, 10));
        b_port = static_cast<unsigned>(std::strtoul(b_value, nullptr, 10));
        ASSERT_NE(a_port, b_port);
    }
    void TearDown() override { serial_close(a); serial_close(b); }
    void open(unsigned stops) {
        unsigned long error;
        a = serial_open(a_port, stops, Inbox::receive, &a_inbox, &error);
        ASSERT_NE(a, nullptr) << error;
        b = serial_open(b_port, stops, Inbox::receive, &b_inbox, &error);
        ASSERT_NE(b, nullptr) << error;
    }
    bool wait_transmitted(size_t count) {
        auto deadline = GetTickCount64() + 3000;
        do {
            if (serial_get_status(a).transmitted == count &&
                serial_get_status(b).transmitted == count) return true;
            Sleep(1);
        } while (GetTickCount64() < deadline);
        return false;
    }
};

TEST_F(SerialPair, FullDuplexUnicodeAndExclusiveAccess)
{
    ASSERT_NO_FATAL_FAILURE(open(1));
    unsigned long error;
    auto duplicate = serial_open(a_port, 1, discard, nullptr, &error);
    EXPECT_EQ(duplicate, nullptr);
    serial_close(duplicate);
    const std::vector<uint32_t> text{'A', 0x42f, 0x20ac, 0x1f642, '\n'};
    for (auto scalar : text) {
        ASSERT_TRUE(serial_send(a, scalar, &error)) << error;
        ASSERT_TRUE(serial_send(b, scalar, &error)) << error;
    }
    ASSERT_TRUE(a_inbox.wait(text.size()));
    ASSERT_TRUE(b_inbox.wait(text.size()));
    ASSERT_TRUE(wait_transmitted(text.size()));
    /* Join producers before inspecting their vectors or destroying callback contexts. */
    auto a_status = serial_get_status(a), b_status = serial_get_status(b);
    serial_close(a); a = nullptr;
    serial_close(b); b = nullptr;
    EXPECT_EQ(a_inbox.text, text);
    EXPECT_EQ(b_inbox.text, text);
    EXPECT_EQ(a_status.error, 0u);
    EXPECT_EQ(b_status.error, 0u);
    EXPECT_EQ(a_status.transmitted, text.size());
    EXPECT_EQ(b_status.transmitted, text.size());
}

TEST_F(SerialPair, TwoStopBitsAndCancellationOfIdleReads)
{
    ASSERT_NO_FATAL_FAILURE(open(2));
    unsigned long error;
    ASSERT_TRUE(serial_send(a, 'Z', &error));
    ASSERT_TRUE(b_inbox.wait(1));
    auto started = std::chrono::steady_clock::now();
    serial_close(a); a = nullptr;
    serial_close(b); b = nullptr;
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(3));
    EXPECT_EQ(b_inbox.text, (std::vector<uint32_t>{'Z'}));
}
