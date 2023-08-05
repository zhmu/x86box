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

        constexpr std::array all_tests{
            TestVector{"add8.bin", "add", Test8x8{ [](auto& flags, auto a, auto b) {
                return cpu::alu::ADD<8>(flags, a, b);
            } } },
            TestVector{"sub8.bin", "sub", Test8x8{ [](auto& flags, auto a, auto b) {
                return cpu::alu::SUB<8>(flags, a, b);
            } } },
            TestVector{"adc8.bin", "adc", Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
                return cpu::alu::ADC<8>(flags, a, b);
            } } },
            TestVector{"sbb8.bin", "sbb", Test8x8WithCarry{ [](auto& flags, auto a, auto b) {
                return cpu::alu::SBB<8>(flags, a, b);
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
                return cpu::alu::OR<8>(flags, a, b);
            } } },
            TestVector{"and8.bin", "and8", Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
                return cpu::alu::AND<8>(flags, a, b);
            } } },
            TestVector{"xor8.bin", "xor8", Test8x8{ [](auto& flags, auto a, auto b) -> uint8_t {
                return cpu::alu::XOR<8>(flags, a, b);
            } } },
            TestVector{"inc8.bin", "inc", Test8WithCarry{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::INC<8>(flags, a);
            } } },
            TestVector{"dec8.bin", "dec", Test8WithCarry{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::DEC<8>(flags, a);
            } } },
            TestVector{"neg8.bin", "neg", Test8{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::NEG<8>(flags, a);
            } } },
            TestVector{"daa.bin", "daa", Test8WithCarryAndAuxCarry{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::DAA(flags, a);
            } } },
            TestVector{"das.bin", "das", Test8WithCarryAndAuxCarry{ [](auto& flags, auto a) -> uint8_t {
                return cpu::alu::DAS(flags, a);
            } } },
            TestVector{"aaa.bin", "aaa", Test16WithAuxCarry{ [](auto& flags, auto a) -> uint16_t {
                return cpu::alu::AAA(flags, a);
            } } },
            TestVector{"aas.bin", "aas", Test16WithAuxCarry{ [](auto& flags, auto a) -> uint16_t {
                return cpu::alu::AAS(flags, a);
            } } },
            TestVector{"aam.bin", "aam", Test8x8WithIntn{ [](auto& flags, auto& intn, auto a, auto b) -> uint16_t {
                uint16_t ax = a;
                const auto result = cpu::alu::AAM(flags, ax, b);
                if (!result) {
                    intn = 0;
                    return ax;
                }
                return *result;
            } } },
            TestVector{"aad.bin", "aad", Test8x8To16{ [](auto& flags, auto a, auto b) -> uint16_t {
                return cpu::alu::AAD(flags, a, b);
            } } },
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

            const auto test_file = "../../../../test/cpu/alu/vectors/"s + std::string(datafile);
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
