#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "cpu_helper.h"

namespace
{
    struct Flags : cpu_helper::Test
    {
    };
}

TEST_F(Flags, HighNibbleBitsAreSet)
{
    th
        .Execute({{
            0x9c, 0x58 /* pushf, pop ax */
        }})
        .VerifyAX(0xf002);
}

TEST_F(Flags, HighNibbleBitsCannotBeCleared)
{
    th
        .Execute({{
            0x31, 0xdb, /* xor bx,bx */
            0x53,       /* push bx */
            0x9d,       /* popf */
            0x9c,       /* pushf */
            0x58        /* pop ax */
        }})
        .VerifyAX(0xf002);
}