#include "alu.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <tuple>
#include <variant>

using namespace std::literals::string_literals;

namespace
{
    struct TestInput8 {
        uint8_t result;
        cpu::Flags flags;
    };

    template<typename T>
    bool TryRead(std::ifstream& ifs, T& result)
    {
        ifs.read(reinterpret_cast<char*>(&result), sizeof(T));
        return ifs.gcount() == sizeof(T);
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

    std::vector<TestInput8> ReadTestData(std::ifstream& ifs, unsigned int amount)
    {
        std::vector<TestInput8> result;
        for (unsigned int n = 0; n < amount; ++n) {
            TestInput8 ti;
            if (!TryRead(ifs, ti.result) || !TryRead(ifs, ti.flags))
                throw std::runtime_error("read error");
            result.push_back(ti);
        }
        return result;
    }

    std::vector<TestInput8> LoadTests8x8(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result = ReadTestData(ifs, 256 * 256);
        if (result.size() != 65536)
            throw std::runtime_error("invalid number of tests in '"s + path + "'");
        return result;
    }

    std::pair<std::vector<TestInput8>, std::vector<TestInput8>> LoadTests8x8WithCarry(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result_without_carry = ReadTestData(ifs, 256 * 256);
        if (result_without_carry.size() != 65536)
            throw std::runtime_error("invalid number of tests without carry in '"s + path + "'");
        auto result_with_carry = ReadTestData(ifs, 256 * 256);
        if (result_with_carry.size() != 65536)
            throw std::runtime_error("invalid number of tests with carry in '"s + path + "'");

        return { result_without_carry, result_with_carry };
    }

    std::vector<TestInput8> LoadTests8(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result = ReadTestData(ifs, 256);
        if (result.size() != 256)
            throw std::runtime_error("invalid number of tests in '"s + path + "'");
        return result;
    }

    template<typename Fn>
    int VerifyOp8x8(const std::vector<TestInput8>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8x8 bit input, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        std::cout << std::hex;
        for(unsigned int a = 0; a <= 255; ++a) {
            for(unsigned int b = 0; b <= 255; ++b) {
                auto flags = initial_flags;
                const auto result = op(flags, a, b);

                const auto [ expected_result, expected_flags ] = tests[a * 256 + b];
                if (expected_result == result && expected_flags == flags)
                    continue;

                ++num_errors;

                std::cout
                    << "*** ERROR: " << static_cast<uint32_t>(a)
                                 << " " << op_text << " "
                                 << static_cast<uint32_t>(b)
                                 << " initial flags "
                                 << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(flags)
                                 << " " << DecodeFlags(flags)
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
                                       << op_text << " "
                                       << static_cast<uint32_t>(b)
                                       << " = " << static_cast<uint32_t>(result)
                                       << " flags "
                                       << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(flags)
                                       << " " << DecodeFlags(flags)
                                       << '\n'
                    << "  expected:  " << static_cast<uint32_t>(a) << " "
                                       << op_text << " "
                                       << static_cast<uint32_t>(b)
                                       << " = " << static_cast<uint32_t>(expected_result)
                                       << " flags "
                                       << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(expected_flags)
                                       << " " << DecodeFlags(expected_flags)
                                       << '\n'
                    << '\n';

            }
        }
        std::cout << std::dec;
        return num_errors;
    }

    template<typename Fn>
    int VerifyOp8(const std::vector<TestInput8>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8 bit input, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        std::cout << std::hex;
        for(unsigned int a = 0; a <= 255; ++a) {
            auto flags = initial_flags;
            const auto result = op(flags, a);

            const auto [ expected_result, expected_flags ] = tests[a];
            if (expected_result == result && expected_flags == flags)
                continue;

            ++num_errors;

            std::cout
                << "*** ERROR: " << static_cast<uint32_t>(a)
                                << " " << op_text
                                << " initial flags "
                                << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(flags)
                                << " " << DecodeFlags(flags)
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
                                    << op_text
                                    << " = " << static_cast<uint32_t>(result)
                                    << " flags "
                                    << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(flags)
                                    << " " << DecodeFlags(flags)
                                    << '\n'
                << "  expected:  " << static_cast<uint32_t>(a) << " "
                                    << op_text
                                    << " = " << static_cast<uint32_t>(expected_result)
                                    << " flags "
                                    << std::setfill('0') << std::setw(4) << static_cast<uint32_t>(expected_flags)
                                    << " " << DecodeFlags(expected_flags)
                                    << '\n'
                << '\n';

        }
        std::cout << std::dec;
        return num_errors;
    }

    template<typename... Ts>
    struct overload : Ts... { using Ts::operator()...; };
}

int main(int argc, char* argv[])
{
    int num_errors = 0;

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
    using TestType = std::variant<
        Test8x8, Test8x8WithCarry, Test8
    >;

    using TestVector = std::tuple<std::string_view, std::string_view, TestType>;

    constexpr std::array tests{
        TestVector{"add8.bin", "add", Test8x8{ [](auto& flags, auto a, auto b) {
            return cpu::alu::Add8(flags, a, b);
        } } },
        TestVector{"sub8.bin", "sub", Test8x8{ [](auto& flags, auto a, auto b) {
            return cpu::alu::Sub8(flags, a, b);
        } } },
        TestVector{"adc8.bin", "adc", Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
            return cpu::alu::Adc8(flags, a, b);
        } } },
        TestVector{"sbb8.bin", "sbb", Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
            return cpu::alu::Sbb8(flags, a, b);
        } } },
        TestVector{"shl8_1.bin", "shl1", Test8{ [](auto& flags, auto a) -> uint8_t {
            return cpu::alu::SHL<8>(flags, a, 1);
        } } }
    };

    for (const auto& [ datafile, test_name, test_data ]: tests)
    {
        const auto test_file = "../../test/"s + std::string(datafile);

        num_errors += std::visit(overload{
            [&](const Test8x8& test) {
                const auto test_data = LoadTests8x8(test_file);
                return VerifyOp8x8(test_data, test_name, test.fn, cpu::flag::ON);
            },
            [&](const Test8x8WithCarry& test) {
                const auto [ test_data_without_carry, test_data_with_carry]  = LoadTests8x8WithCarry(test_file);
                return
                    VerifyOp8x8(test_data_without_carry, test_name, test.fn, cpu::flag::ON) +
                    VerifyOp8x8(test_data_with_carry, test_name, test.fn, cpu::flag::ON | cpu::flag::CF);
            },
            [&](const Test8& test) {
                const auto test_data = LoadTests8(test_file);
                return VerifyOp8(test_data, test_name, test.fn, cpu::flag::ON);
            },
        }, test_data);
    }

    if (num_errors == 0) {
        std::cout << "Everything OK!\n";
        return 0;
    } else {
        std::cout << "Failure, " << num_errors << " error(s) encountered\n";
        return 1;
    }
}