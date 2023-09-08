#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "cpu_helper.h"

namespace
{
    struct Jump  : cpu_helper::Test
    {
    };

    void VerifyAXIsOne(cpu_helper::TestHelper& th) { th.VerifyAX(1); }
    void VerifyAXIsZero(cpu_helper::TestHelper& th) { th.VerifyAX(0); }
}

TEST_F(Jump, JA_JNBE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x77, 0x03,         // ja +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsZero },
        { [](auto& th) { th.CF(); }, VerifyAXIsZero },
        { [](auto& th) { th.ZF().CF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JAE_JNB_JNC)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x73, 0x03,         // jnc +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.CF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JB_JC_JNAE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x72, 0x03,         // jc +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.CF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JBE_JNA)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x76, 0x03,         // jna +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.CF(); }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsOne },
        { [](auto& th) { th.ZF().CF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JCXZ)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0xe3, 0x03,         // jcxz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { th.CX(0); }, VerifyAXIsOne },
        { [](auto& th) { th.CX(1); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JE_JZ)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x74, 0x03,         // jz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.ZF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JG_JNLE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7f, 0x03,         // jg +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsZero },
        { [](auto& th) { th.SF(); }, VerifyAXIsZero },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsZero },
        { [](auto& th) { th.ZF().OF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JGE_JNL)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7d, 0x03,         // jge +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.SF(); }, VerifyAXIsZero },
        { [](auto& th) { th.OF(); }, VerifyAXIsZero },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JL_JNGE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7c, 0x03,         // jl +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.SF(); }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsOne },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JLE_JNG)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7e, 0x03,         // jl +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.ZF(); }, VerifyAXIsOne },
        { [](auto& th) { th.SF(); }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsOne },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsZero },
        { [](auto& th) { th.ZF().SF().OF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JNE_JNZ)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x75, 0x03,         // jnz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JNO)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x71, 0x03,         // jno +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JNP_JPO)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7b, 0x03,         // jnp +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.PF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JNS)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x79, 0x03,         // jns +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.SF(); }, VerifyAXIsZero },
    }});
}

TEST_F(Jump, JO)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x70, 0x03,         // jo +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.OF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JP_JPE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7a, 0x03,         // jp +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.PF(); }, VerifyAXIsOne },
    }});
}

TEST_F(Jump, JS)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x78, 0x03,         // js +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.SF(); }, VerifyAXIsOne },
    }});
}