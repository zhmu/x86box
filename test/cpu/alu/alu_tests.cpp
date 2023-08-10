#include "../src/cpu/alu.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <tuple>
#include <variant>
#include <algorithm>
#include <optional>
#include <type_traits>
#include <unistd.h>

#define GTEST_DONT_DEFINE_TEST 1
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace std::literals::string_literals;

namespace
{
    template<typename T>
    struct TestInput {
        T result;
        cpu::Flags flags;
    };

    using TestInput8 = TestInput<uint8_t>;
    using TestInput16 = TestInput<uint16_t>;

    struct TestInputIntn8 {
        uint16_t result;
        cpu::Flags flags;
        uint8_t intn;
    };

    template<typename T>
    requires std::is_integral_v<T>
    bool TryRead(std::ifstream& ifs, T& result)
    {
        ifs.read(reinterpret_cast<char*>(&result), sizeof(T));
        return ifs.gcount() == sizeof(T);
    }

    bool TryRead(std::ifstream& ifs, TestInput8& ti)
    {
        return TryRead(ifs, ti.result) && TryRead(ifs, ti.flags);
    }

    bool TryRead(std::ifstream& ifs, TestInput16& ti)
    {
        return TryRead(ifs, ti.result) && TryRead(ifs, ti.flags);
    }

    bool TryRead(std::ifstream& ifs, TestInputIntn8& ti)
    {
        return TryRead(ifs, ti.result) && TryRead(ifs, ti.flags) && TryRead(ifs, ti.intn);
    }

    std::string DecodeFlags(const cpu::Flags flags)
    {
        using FlagCombination  = std::pair<cpu::Flags, char>;
        constexpr std::array flagBits{
            FlagCombination{cpu::flag::OF, 'O'},
            FlagCombination{cpu::flag::SF, 'S'},
            FlagCombination{cpu::flag::ZF, 'Z'},
            FlagCombination{cpu::flag::AF, 'A'},
            FlagCombination{cpu::flag::PF, 'P'},
            FlagCombination{cpu::flag::ON, '1'},
            FlagCombination{cpu::flag::CF, 'C'},
        };

        std::string s(12, '.');
        for(const auto& [ flag, ch ]: flagBits) {
            if (flags & flag) {
                const auto index = s.size() - 1 - std::countr_zero(flag);
                s[index] = ch;
            }
        }
        return s;
    }

    template<typename T>
    std::vector<T> ReadTestData(std::ifstream& ifs, unsigned int amount)
    {
        std::vector<T> result;
        for (unsigned int n = 0; n < amount; ++n) {
            T ti;
            if (!TryRead(ifs, ti))
                throw std::runtime_error("read error");
            result.push_back(ti);
        }
        if (result.size() != amount)
            throw std::runtime_error("insufficient number of tests encountered");
        return result;
    }
    auto ReadTestData8(std::ifstream& ifs, unsigned int amount) { return ReadTestData<TestInput8>(ifs, amount); }
    auto ReadTestData16(std::ifstream& ifs, unsigned int amount) { return ReadTestData<TestInput16>(ifs, amount); }
    auto ReadTestDataIntn8(std::ifstream& ifs, unsigned int amount) { return ReadTestData<TestInputIntn8>(ifs, amount); }

    std::vector<TestInput8> LoadTests8x8(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result = ReadTestData8(ifs, 256 * 256);
        return result;
    }

    std::pair<std::vector<TestInput8>, std::vector<TestInput8>> LoadTests8x8WithCarry(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result_without_carry = ReadTestData8(ifs, 256 * 256);
        auto result_with_carry = ReadTestData8(ifs, 256 * 256);
        return { result_without_carry, result_with_carry };
    }

    std::vector<TestInput8> LoadTests8(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result = ReadTestData8(ifs, 256);
        return result;
    }

    std::vector<TestInput16> LoadTests8x8To16(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result = ReadTestData16(ifs, 65536);
        return result;
    }

    std::vector<TestInputIntn8> LoadTestsIntn8x8(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        return ReadTestDataIntn8(ifs, 256 * 256);
    }

    std::pair<std::vector<TestInput8>, std::vector<TestInput8>> LoadTests8WithCarry(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result_without_carry = ReadTestData8(ifs, 256);
        auto result_with_carry = ReadTestData8(ifs, 256);
        return { result_without_carry, result_with_carry };
    }

    std::tuple<std::vector<TestInput8>, std::vector<TestInput8>, std::vector<TestInput8>, std::vector<TestInput8>> LoadTests8WithCarryAndAuxCarry(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result_0 = ReadTestData8(ifs, 256);
        auto result_cf = ReadTestData8(ifs, 256);
        auto result_af = ReadTestData8(ifs, 256);
        auto result_cf_af = ReadTestData8(ifs, 256);
        return { result_0, result_cf, result_af, result_cf_af };
    }

    std::pair<std::vector<TestInput16>, std::vector<TestInput16>> LoadTests16WithCarry(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result_without_carry = ReadTestData16(ifs, 65536);
        auto result_with_carry = ReadTestData16(ifs, 65536);
        return { result_without_carry, result_with_carry };
    }

    int ProcessTestResult(std::string_view op_text, uint32_t a, std::optional<uint32_t> b, cpu::Flags initial_flags, uint32_t result, cpu::Flags flags, uint8_t intn, uint32_t expected_result, cpu::Flags expected_flags, uint8_t expected_intn)
    {
        if (expected_result == result && expected_flags == flags && expected_intn == intn)
            return 0;

        std::cout << std::hex;
        std::cout
            << "*** ERROR: " << static_cast<uint32_t>(a)
            << " " << op_text;
        if (b) {
            std::cout << " " << static_cast<uint32_t>(*b);
        };
        std::cout
            << " initial flags "
            << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(initial_flags)
            << " " << DecodeFlags(initial_flags)
            << '\n';

        if (expected_result != result)
        {
            std::cout
                    << "  !! RESULT MISMATCH: "
                    << " got " << static_cast<uint32_t>(result)
                    << " expected " << static_cast<uint32_t>(expected_result)
                    << "\n";
        }

        if (expected_flags != flags)
        {
            std::cout
                    << "  !! FLAGS MISMATCH: "
                    << " got " << static_cast<uint32_t>(flags)
                    << " expected " << static_cast<uint32_t>(expected_flags)
                    << "\n";
        }

        std::cout
            << "  result  :  " << static_cast<uint32_t>(a) << " "
                               << op_text;
        if (b) {
            std::cout << " " << static_cast<uint32_t>(*b);
        }
        std::cout
            << " = " << static_cast<uint32_t>(result)
            << " flags "
            << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(flags)
            << " " << DecodeFlags(flags)
            << '\n'
            << "  expected:  " << static_cast<uint32_t>(a) << " "
            << op_text;
        if (b) {
            std::cout << " " << static_cast<uint32_t>(*b);
        }
        std::cout
            << " = " << static_cast<uint32_t>(expected_result)
            << " flags "
            << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(expected_flags)
            << " " << DecodeFlags(expected_flags)
            << "\n\n";
        std::cout << std::dec;
        return 1;
    }

    template<typename Fn>
    int VerifyOp8x8(const std::vector<TestInput8>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8x8 bit input, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        for(unsigned int a = 0; a <= 255; ++a) {
            for(unsigned int b = 0; b <= 255; ++b) {
                auto flags = initial_flags;
                const auto result = op(flags, a, b);

                const auto [ expected_result, expected_flags ] = tests[a * 256 + b];
                num_errors += ProcessTestResult(op_text, a, b, initial_flags, result, flags, 0, expected_result, expected_flags, 0);
            }
        }
        return num_errors;
    }

    template<typename Fn>
    int VerifyOp8x8Intn(const std::vector<TestInputIntn8>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8x8 bit input, potential interrupt, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        for(unsigned int a = 0; a <= 255; ++a) {
            for(unsigned int b = 0; b <= 255; ++b) {
                auto flags = initial_flags;
                uint8_t intn = 0xff;
                const auto result = op(flags, intn, a, b);

                const auto [ expected_result, expected_flags, expected_intn ] = tests[a * 256 + b];
                num_errors += ProcessTestResult(op_text, a, b, initial_flags, result, flags, intn, expected_result, expected_flags, expected_intn);
            }
        }
        return num_errors;
    }

    template<typename Fn>
    int VerifyOp8x8To16(const std::vector<TestInput16>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8x8 bit input, 16 bit output, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        for(unsigned int a = 0; a <= 255; ++a) {
            for(unsigned int b = 0; b <= 255; ++b) {
                auto flags = initial_flags;
                const auto result = op(flags, a, b);

                const auto [ expected_result, expected_flags ] = tests[a * 256 + b];
                num_errors += ProcessTestResult(op_text, a, b, initial_flags, result, flags, 0, expected_result, expected_flags, 0);
            }
        }
        return num_errors;
    }

    template<typename Fn>
    int VerifyOp8(const std::vector<TestInput8>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8 bit input, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        for(unsigned int a = 0; a <= 255; ++a) {
            auto flags = initial_flags;
            const auto result = op(flags, a);

            const auto [ expected_result, expected_flags ] = tests[a];
            num_errors += ProcessTestResult(op_text, a, {}, initial_flags, result, flags, 0, expected_result, expected_flags, 0);
        }
        return num_errors;
    }

    template<typename Fn>
    int VerifyOp16(const std::vector<TestInput16>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (16 bit input, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        for(unsigned int a = 0; a <= 65535; ++a) {
            auto flags = initial_flags;
            const auto result = op(flags, a);

            const auto [ expected_result, expected_flags ] = tests[a];
            num_errors += ProcessTestResult(op_text, a, {}, initial_flags, result, flags, 0, expected_result, expected_flags, 0);
        }
        return num_errors;
    }

    template<typename... Ts>
    struct overload : Ts... { using Ts::operator()...; };

    namespace tests
    {
        using TestFn8x8 = uint8_t(*)(cpu::Flags&, uint8_t, uint8_t);
        struct Test8x8 {
            TestFn8x8 fn;
        };
        struct Test8x8WithCarry {
            TestFn8x8 fn;
        };

        using TestFn8 = uint8_t(*)(cpu::Flags&, uint8_t);
        struct Test8 {
            TestFn8 fn;
        };
        struct Test8WithCarry {
            TestFn8 fn;
        };
        struct Test8WithCarryAndAuxCarry {
            TestFn8 fn;
        };
        struct Test8WithAuxCarry {
            TestFn8 fn;
        };
        using TestFn16 = uint16_t(*)(cpu::Flags&, uint16_t);
        struct Test16WithAuxCarry {
            TestFn16 fn;
        };
        using TestFnIntn8x8 = uint16_t(*)(cpu::Flags&, uint8_t&, uint8_t, uint8_t);
        struct Test8x8WithIntn {
            TestFnIntn8x8 fn;
        };
        using TestFn8x8To16 = uint16_t(*)(cpu::Flags&, uint8_t, uint8_t);
        struct Test8x8To16 {
            TestFn8x8To16 fn;
        };
        using TestType = std::variant<
            Test8x8, Test8x8WithCarry, Test8, Test8WithCarry, Test8WithCarryAndAuxCarry, Test8WithAuxCarry, Test16WithAuxCarry,
            Test8x8WithIntn, Test8x8To16
        >;

        using TestVector = std::tuple<std::string_view, std::string_view, TestType>;
    }

    int RunTest(const tests::TestVector& test_vector)
    {
        const auto& [ datafile, test_name, test_data ] = test_vector;
        const auto test_file = "../../../../test/cpu/alu/vectors/"s + std::string(datafile);
        return std::visit(overload{
            [&](const tests::Test8x8& test) {
                const auto test_data = LoadTests8x8(test_file);
                return VerifyOp8x8(test_data, test_name, test.fn, cpu::flag::ON);
            },
            [&](const tests::Test8x8WithCarry& test) {
                const auto [ test_data_without_carry, test_data_with_carry]  = LoadTests8x8WithCarry(test_file);
                return
                    VerifyOp8x8(test_data_without_carry, test_name, test.fn, cpu::flag::ON) +
                    VerifyOp8x8(test_data_with_carry, test_name, test.fn, cpu::flag::ON | cpu::flag::CF);
            },
            [&](const tests::Test8& test) {
                const auto test_data = LoadTests8(test_file);
                return VerifyOp8(test_data, test_name, test.fn, cpu::flag::ON);
            },
            [&](const tests::Test8WithCarry& test) {
                const auto [ test_data_without_carry, test_data_with_carry]  = LoadTests8WithCarry(test_file);
                return
                    VerifyOp8(test_data_without_carry, test_name, test.fn, cpu::flag::ON) +
                    VerifyOp8(test_data_with_carry, test_name, test.fn, cpu::flag::ON | cpu::flag::CF);
            },
            [&](const tests::Test8WithAuxCarry& test) {
                const auto [ test_data_0, test_data_af ]  = LoadTests8WithCarry(test_file);
                return
                    VerifyOp8(test_data_0, test_name, test.fn, cpu::flag::ON) +
                    VerifyOp8(test_data_af, test_name, test.fn, cpu::flag::ON | cpu::flag::AF);
            },
            [&](const tests::Test8WithCarryAndAuxCarry& test) {
                const auto [ test_data_0, test_data_cf, test_data_af, test_data_cf_af ]  = LoadTests8WithCarryAndAuxCarry(test_file);
                return
                    VerifyOp8(test_data_0, test_name, test.fn, cpu::flag::ON) +
                    VerifyOp8(test_data_cf, test_name, test.fn, cpu::flag::ON | cpu::flag::CF) +
                    VerifyOp8(test_data_af, test_name, test.fn, cpu::flag::ON | cpu::flag::AF) +
                    VerifyOp8(test_data_cf_af, test_name, test.fn, cpu::flag::ON | cpu::flag::CF | cpu::flag::AF);
            },
            [&](const tests::Test16WithAuxCarry& test) {
                const auto [ test_data_0, test_data_af ]  = LoadTests16WithCarry(test_file);
                return
                    VerifyOp16(test_data_0, test_name, test.fn, cpu::flag::ON) +
                    VerifyOp16(test_data_af, test_name, test.fn, cpu::flag::ON | cpu::flag::AF);
            },
            [&](const tests::Test8x8WithIntn& test) {
                const auto test_data = LoadTestsIntn8x8(test_file);
                return
                    VerifyOp8x8Intn(test_data, test_name, test.fn, cpu::flag::ON);
            },
            [&](const tests::Test8x8To16& test) {
                const auto test_data = LoadTests8x8To16(test_file);
                return VerifyOp8x8To16(test_data, test_name, test.fn, cpu::flag::ON);
            },
        }, test_data);
    }
}

GTEST_TEST(ALUTest, Add)
{
    const auto num_errors = RunTest({"add8.bin", "add", tests::Test8x8{ [](auto& flags, auto a, auto b) {
        return cpu::alu::ADD<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Sub)
{
    const auto num_errors = RunTest({"sub8.bin", "sub", tests::Test8x8{ [](auto& flags, auto a, auto b) {
        return cpu::alu::SUB<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Adc)
{
    const auto num_errors = RunTest({"adc8.bin", "adc", tests::Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
                return cpu::alu::ADC<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Sbb)
{
    const auto num_errors = RunTest({"sbb8.bin", "sbb", tests::Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
        return cpu::alu::SBB<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}


GTEST_TEST(ALUTest, Shl8_1)
{
    const auto num_errors = RunTest({"shl8_1.bin", "shl1", tests::Test8{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::SHL<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Shl8_8)
{
    const auto num_errors = RunTest({"shl8_8.bin", "shl", tests::Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
        return cpu::alu::SHL<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Shr8_1)
{
    const auto num_errors = RunTest({"shr8_1.bin", "shr1", tests::Test8{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::SHR<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Shr8_8)
{
    const auto num_errors = RunTest({"shr8_8.bin", "shr", tests::Test8x8{ [](auto& flags, auto a, auto b) {
        return cpu::alu::SHR<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Sar8_1)
{
    const auto num_errors = RunTest({"sar8_1.bin", "sar1", tests::Test8{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::SAR<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Sar8_8)
{
    const auto num_errors = RunTest({"sar8_8.bin", "sar", tests::Test8x8{ [](auto& flags, auto a, auto b) {
        return cpu::alu::SAR<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Rol8_1)
{
    const auto num_errors = RunTest({"rol8_1.bin", "rol1", tests::Test8{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::ROL<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Rol8)
{
    const auto num_errors = RunTest({"rol8_8.bin", "rol", tests::Test8x8{ [](auto& flags, auto a, auto b) {
        return cpu::alu::ROL<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Ror1)
{
    const auto num_errors = RunTest({"ror8_1.bin", "ror1", tests::Test8{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::ROR<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Ror)
{
    const auto num_errors = RunTest({"ror8_8.bin", "ror", tests::Test8x8{ [](auto& flags, auto a, auto b) {
        return cpu::alu::ROR<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Rcl1)
{
    const auto num_errors = RunTest({"rcl8_1.bin", "rcl1", tests::Test8WithCarry{ [](auto& flags, auto a) {
        return cpu::alu::RCL<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Rcl)
{
    const auto num_errors = RunTest({"rcl8_8.bin", "rcl", tests::Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
        return cpu::alu::RCL<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Rcr1)
{
    const auto num_errors = RunTest({"rcr8_1.bin", "rcr1", tests::Test8WithCarry{ [](auto& flags, auto a) {
        return cpu::alu::RCR<8>(flags, a, 1);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Rcr)
{
    const auto num_errors = RunTest({"rcr8_8.bin", "rcr", tests::Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
        return cpu::alu::RCR<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Or)
{
    const auto num_errors = RunTest({"or8.bin", "or8", tests::Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
        return cpu::alu::OR<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, And)
{
    const auto num_errors = RunTest({"and8.bin", "and8", tests::Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
        return cpu::alu::AND<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Xor)
{
    const auto num_errors = RunTest({"xor8.bin", "xor8", tests::Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
        return cpu::alu::XOR<8>(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Inc)
{
    const auto num_errors = RunTest({"inc8.bin", "inc", tests::Test8WithCarry{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::INC<8>(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Dec)
{
    const auto num_errors = RunTest({"dec8.bin", "dec", tests::Test8WithCarry{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::DEC<8>(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Neg)
{
    const auto num_errors = RunTest({"neg8.bin", "neg", tests::Test8{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::NEG<8>(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Daa)
{
    const auto num_errors = RunTest( {"daa.bin", "daa", tests::Test8WithCarryAndAuxCarry{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::DAA(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Das)
{
    const auto num_errors = RunTest({"das.bin", "das", tests::Test8WithCarryAndAuxCarry{ [](auto& flags, auto a) -> uint8_t {
        return cpu::alu::DAS(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Aaa)
{
    const auto num_errors = RunTest({"aaa.bin", "aaa", tests::Test16WithAuxCarry{ [](auto& flags, auto a) -> uint16_t {
        return cpu::alu::AAA(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Aas)
{
    const auto num_errors = RunTest({"aas.bin", "aas", tests::Test16WithAuxCarry{ [](auto& flags, auto a) -> uint16_t {
        return cpu::alu::AAS(flags, a);
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Aam)
{
    const auto num_errors = RunTest({"aam.bin", "aam", tests::Test8x8WithIntn{ [](auto& flags, auto& intn, auto a, auto b) -> uint16_t {
        uint16_t ax = a;
        const auto result = cpu::alu::AAM(flags, ax, b);
        if (!result) {
            intn = 0;
            return ax;
        }
        return *result;
    } } });
    EXPECT_EQ(num_errors, 0);
}

GTEST_TEST(ALUTest, Aad)
{
    const auto num_errors = RunTest({"aad.bin", "aad", tests::Test8x8To16{ [](auto& flags, auto a, auto b) -> uint16_t {
        return cpu::alu::AAD(flags, a, b);
    } } });
    EXPECT_EQ(num_errors, 0);
}
