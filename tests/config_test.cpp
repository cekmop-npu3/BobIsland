#include "config_win32.h"
#include <gtest/gtest.h>
#include <cstring>

TEST(WindowsConfiguration, ReplacesInheritedFlowControlAndFixesOtherSettings)
{
    DCB dcb;
    std::memset(&dcb, 0xff, sizeof(dcb));
    ASSERT_TRUE(make_dcb(1, &dcb));
    EXPECT_EQ(dcb.DCBlength, sizeof(dcb));
    EXPECT_EQ(dcb.BaudRate, 9600u);
    EXPECT_EQ(dcb.ByteSize, 8);
    EXPECT_EQ(dcb.Parity, NOPARITY);
    EXPECT_EQ(dcb.StopBits, ONESTOPBIT);
    EXPECT_TRUE(dcb.fBinary);
    EXPECT_FALSE(dcb.fParity);
    EXPECT_FALSE(dcb.fOutxCtsFlow);
    EXPECT_FALSE(dcb.fOutxDsrFlow);
    EXPECT_FALSE(dcb.fDsrSensitivity);
    EXPECT_FALSE(dcb.fOutX);
    EXPECT_FALSE(dcb.fInX);
    EXPECT_FALSE(dcb.fNull);
    EXPECT_FALSE(dcb.fErrorChar);
    EXPECT_EQ(static_cast<DWORD>(dcb.fDtrControl), DTR_CONTROL_ENABLE);
    EXPECT_EQ(static_cast<DWORD>(dcb.fRtsControl), RTS_CONTROL_ENABLE);
    EXPECT_TRUE(dcb.fAbortOnError);
    EXPECT_EQ(dcb.wReserved, 0);
    EXPECT_EQ(dcb.wReserved1, 0);
}

TEST(WindowsConfiguration, StopSelectionChangesOnlyStopBits)
{
    DCB one{}, two{};
    ASSERT_TRUE(make_dcb(1, &one));
    ASSERT_TRUE(make_dcb(2, &two));
    EXPECT_EQ(two.StopBits, TWOSTOPBITS);
    two.StopBits = one.StopBits;
    EXPECT_EQ(std::memcmp(&one, &two, sizeof(one)), 0);
    EXPECT_FALSE(make_dcb(15, &two));
}

TEST(WindowsConfiguration, ReadsWaitAndWritesHaveAFiniteTimeout)
{
    COMMTIMEOUTS timeouts{};
    make_timeouts(&timeouts);
    EXPECT_EQ(timeouts.ReadIntervalTimeout, 0u);
    EXPECT_EQ(timeouts.ReadTotalTimeoutMultiplier, 0u);
    EXPECT_EQ(timeouts.ReadTotalTimeoutConstant, 100u);
    EXPECT_EQ(timeouts.WriteTotalTimeoutMultiplier, 0u);
    EXPECT_EQ(timeouts.WriteTotalTimeoutConstant, 2000u);
}
