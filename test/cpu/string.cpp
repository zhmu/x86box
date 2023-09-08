#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "cpu_helper.h"

namespace
{
    struct String : cpu_helper::Test
    {
    };
}

TEST_F(String, STOSB)
{
    th
        .ExpectWriteByte(0x12345, 0x67)
        .ES(0x1000)
        .DI(0x2345)
        .AX(0x67)
        .Execute({{
            0xaa                // stosb
        }});
}

TEST_F(String, STOSW)
{
    th
        .ExpectWriteWord(0xabcde, 0x1378)
        .ES(0xa000)
        .DI(0xbcde)
        .AX(0x1378)
        .Execute({{
            0xab                // stosw
        }});
}

TEST_F(String, LODSB)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadByte(0x12345, 0x1)
            .DS(0x1234)
            .SI(0x5)
            .AX(0xffff);
    }, {{
        0xac                // lodsb
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifyAX(0xff01)
                .VerifySI(0x0006);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifyAX(0xff01)
                .VerifySI(0x0004);
        } }
    }});
}

TEST_F(String, LODSW)
{
    RunTests([](auto& th) {
        th
            .ExpectReadWord(0x1000f, 0x9f03)
            .DS(0x1000)
            .SI(0xf)
            .AX(0xffff);
    }, {{
        0xad                // lodsw
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifyAX(0x9f03)
                .VerifySI(0x0011);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifyAX(0x9f03)
                .VerifySI(0x000d);
        } },
    }});
}

TEST_F(String, MOVSB)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadByte(0x1234a, 0x55)
            .ExpectWriteByte(0xf0027, 0x55)
            .DS(0x1234)
            .SI(0xa)
            .ES(0xf000)
            .DI(0x27);
    }, {{
        0xa4                // movsb
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifySI(0x000b)
                .VerifyDI(0x0028);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifySI(0x0009)
                .VerifyDI(0x0026);
        } }
    }});
}

TEST_F(String, MOVSW)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadWord(0x23459, 0x55aa)
            .ExpectWriteWord(0x00003, 0x55aa)
            .DS(0x2345)
            .SI(0x9)
            .ES(0x0)
            .DI(0x3);
    }, {{
        0xa5                // movsw
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifySI(0x000b)
                .VerifyDI(0x0005);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifySI(0x0007)
                .VerifyDI(0x0001);
        } }
    }});
}

TEST_F(String, CMPSB_Matches)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadByte(0x12345, 0x1)
            .ExpectReadByte(0x23456, 0x1)
            .DS(0x1234)
            .SI(0x5)
            .ES(0x2345)
            .DI(0x6);
    }, {{
        0xa6                // cmpsb
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifySI(0x0006)
                .VerifyDI(0x0007)
                .VerifyZF(true);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifySI(0x0004)
                .VerifyDI(0x0005)
                .VerifyZF(true);
        } }
    }});
}

TEST_F(String, CMPSB_Mismatches)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadByte(0x12345, 0x1)
            .ExpectReadByte(0x23456, 0xfe)
            .DS(0x1234)
            .SI(0x5)
            .ES(0x2345)
            .DI(0x6);
    }, {{
        0xa6                // cmpsb
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifySI(0x0006)
                .VerifyDI(0x0007)
                .VerifyZF(false);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifySI(0x0004)
                .VerifyDI(0x0005)
                .VerifyZF(false);
        } }
    }});
}

TEST_F(String, SCASB_Matches)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadByte(0x3434f, 0x94)
            .ES(0x3430)
            .DI(0x4f)
            .AX(0x94);
    }, {{
        0xae                // scasb
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifyDI(0x0050)
                .VerifyZF(true);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifyDI(0x4e)
                .VerifyZF(true);
        } }
    }});
}

TEST_F(String, SCASB_Mismatches)
{
    RunTests([](auto& th) {
        th
            .ExpectReadByte(cpu_helper::initialIp)
            .ExpectReadByte(0x23900, 0x80)
            .ES(0x2390)
            .DI(0x0)
            .AX(0x94);
    }, {{
        0xae                // scasb
    }}, {{
        { [](auto& th) { }, [](auto& th) {
            th
                .VerifyDI(0x0001)
                .VerifyZF(false);
        } },
        { [](auto& th) { th.DF(); }, [](auto& th) {
            th
                .VerifyDI(0xffff)
                .VerifyZF(false);
        } }
    }});
}