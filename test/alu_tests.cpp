#include "alu.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <tuple>
#include <variant>
#include <algorithm>
#include <unistd.h>

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

    std::pair<std::vector<TestInput8>, std::vector<TestInput8>> LoadTests8WithCarry(const std::string& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        auto result_without_carry = ReadTestData(ifs, 256);
        if (result_without_carry.size() != 256)
            throw std::runtime_error("invalid number of tests without carry in '"s + path + "'");
        auto result_with_carry = ReadTestData(ifs, 256);
        if (result_with_carry.size() != 256)
            throw std::runtime_error("invalid number of tests with carry in '"s + path + "'");

        return { result_without_carry, result_with_carry };
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
        using TestType = std::variant<
            Test8x8, Test8x8WithCarry, Test8, Test8WithCarry
        >;

        using TestVector = std::tuple<std::string_view, std::string_view, TestType>;

        constexpr std::array all_tests{
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
            } } },
            TestVector{"shl8_8.bin", "shl", Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
                return cpu::alu::SHL<8>(flags, a, b);
            }, } },
            TestVector{"shr8_1.bin", "shr1", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::SHR<8>(flags, a, 1);
            } } },
            TestVector{"shr8_8.bin", "shr", Test8x8{ [](auto& flags, auto a, auto b) {
                return cpu::alu::SHR<8>(flags, a, b);
            } } },
            TestVector{"sar8_1.bin", "sar1", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::SAR<8>(flags, a, 1);
            } } },
            TestVector{"sar8_8.bin", "sar", Test8x8{ [](auto& flags, auto a, auto b) {
                return cpu::alu::SAR<8>(flags, a, b);
            } } },
            TestVector{"rol8_1.bin", "rol1", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::ROL<8>(flags, a, 1);
            } } },
            TestVector{"rol8_8.bin", "rol", Test8x8{ [](auto& flags, auto a, auto b) {
                return cpu::alu::ROL<8>(flags, a, b);
            } } },
            TestVector{"ror8_1.bin", "ror1", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::ROR<8>(flags, a, 1);
            } } },
            TestVector{"ror8_8.bin", "ror", Test8x8{ [](auto& flags, auto a, auto b) {
                return cpu::alu::ROR<8>(flags, a, b);
            } } },
            TestVector{"rcl8_1.bin", "rcl1", Test8WithCarry{ [](auto& flags, auto a) {
                return cpu::alu::RCL<8>(flags, a, 1);
            } } },
            TestVector{"rcl8_8.bin", "rcl", Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
                return cpu::alu::RCL<8>(flags, a, b);
            } } },
            TestVector{"rcr8_1.bin", "rcr1", Test8WithCarry{ [](auto& flags, auto a) {
                return cpu::alu::RCR<8>(flags, a, 1);
            } } },
            TestVector{"rcr8_8.bin", "rcr", Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
                return cpu::alu::RCR<8>(flags, a, b);
            } } },
            TestVector{"or8.bin", "or8", Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
                return cpu::alu::Or8(flags, a, b);
            } } },
            TestVector{"and8.bin", "and8", Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
                return cpu::alu::And8(flags, a, b);
            } } },
            TestVector{"xor8.bin", "xor8", Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
                return cpu::alu::Xor8(flags, a, b);
            } } },
            TestVector{"inc8.bin", "inc", Test8WithCarry{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Inc8(flags, a);
            } } },
            TestVector{"dec8.bin", "dec", Test8WithCarry{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Dec8(flags, a);
            } } },
    #if 0
            TestVector{"neg8.bin", "neg", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Neg8(flags, a);
            } } },
            TestVector{"daa.bin", "daa", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Daa(flags, a);
            } } },
            TestVector{"das.bin", "das", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Das(flags, a);
            } } },
            TestVector{"aaa.bin", "aaa", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Aaa(flags, a);
            } } },
            TestVector{"aas.bin", "aas", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::Aas(flags, a);
            } } },
    #endif
        };
    }

    bool IsTestActiveInFilter(const std::vector<std::string>& filter, std::string_view test_name)
    {
        if (filter.empty()) return true;
        return std::find_if(filter.begin(), filter.end(), [&](const auto& s) { return s == test_name; }) != filter.end();
    }

    int RunTests(const std::vector<std::string>& filter)
    {
        int num_errors = 0;
        for (const auto& [ datafile, test_name, test_data ]: tests::all_tests) {
            if (!IsTestActiveInFilter(filter, test_name)) continue;

            const auto test_file = "../../test/"s + std::string(datafile);
            num_errors += std::visit(overload{
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
            }, test_data);
        }
        return num_errors;
    }

    void ListTests()
    {
        for (const auto& [ datafile, test_name, test_data ]: tests::all_tests) {
            std::cout << test_name << '\n';
        }
    }
}

int main(int argc, char* argv[])
{
    std::vector<std::string> enabled_tests;
    while (true) {
        const int opt = getopt(argc, argv, "l");
        if (opt < 0) break;
        switch(opt) {
            case 'l':
                ListTests();
                return 0;
            default:
                std::cerr << "usage: " << argv[0] << " -l tests...\n"
                          << "\n"
                          << "  -l     list all available tests\n"
                          << '\n'
                          << "provide test names to run only test tests\n"
                          << "the entire test name must match\n";
                return EXIT_FAILURE;
        }
    }

    for(; optind < argc; ++optind) {
        std::string test_name{ argv[optind] };
        if (std::find_if(tests::all_tests.begin(), tests::all_tests.end(), [&](const auto& v) { return std::get<1>(v) == test_name; }) == tests::all_tests.end()) {
            std::cout << "test '" << test_name << "' unrecognized, aborting\n";
            return EXIT_FAILURE;
        }
        enabled_tests.push_back(std::move(test_name));
    }

    const auto num_errors = RunTests(enabled_tests);
    if (num_errors == 0) {
        std::cout << "Everything OK!\n";
        return 0;
    } else {
        std::cout << "Failure, " << num_errors << " error(s) encountered\n";
        return 1;
    }
}