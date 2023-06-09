#include "alu.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>

using namespace std::literals::string_literals;

namespace
{
    struct TestInput8x8 {
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

    std::vector<TestInput8x8> LoadTests8x8(const char* path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) throw std::runtime_error("cannot open '"s + path + "'");

        std::vector<TestInput8x8> result;
        for (unsigned int n = 0; n < 65536; ++n) {
            TestInput8x8 ti;
            if (!TryRead(ifs, ti.result) || !TryRead(ifs, ti.flags))
                throw std::runtime_error("read error");
            result.push_back(ti);
        }

        if (result.size() != 65536)
            throw std::runtime_error("invalid number of tests in '"s + path + "'");
        return result;
    }

    template<typename Fn>
    int VerifyOp8x8(const std::vector<TestInput8x8>& tests, std::string_view op_text, Fn op, cpu::Flags initial_flags)
    {
        std::cout << "Testing " << op_text << " (8x8 bit input, initial flags: " << DecodeFlags(initial_flags) << ")\n";

        int num_errors = 0;
        for(unsigned int a = 0; a <= 255; ++a) {
            for(unsigned int b = 0; b <= 255; ++b) {
                auto flags = initial_flags;
                const auto result = op(flags, a, b);

                const auto [ expected_result, expected_flags ] = tests[a * 256 + b];
                if (expected_result == result && expected_flags == flags)
                    continue;

                ++num_errors;
                std::cout << std::hex;

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
        return num_errors;
    }
}

int main(int argc, char* argv[])
{
    int num_errors = 0;

    if (const auto addTests = LoadTests8x8("../../test/add8.bin"); true)
        num_errors += VerifyOp8x8(addTests, "add", cpu::alu::Add8, cpu::flag::ON);
    if (const auto subTests = LoadTests8x8("../../test/sub8.bin"); true)
        num_errors += VerifyOp8x8(subTests, "sub", cpu::alu::Sub8, cpu::flag::ON);

    if (num_errors == 0) {
        std::cout << "Everything OK!\n";
        return 0;
    } else {
        std::cout << "Failure, " << num_errors << " error(s) encountered\n";
        return 1;
    }
}