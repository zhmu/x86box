#include "gtest/gtest.h"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::err);
    spdlog::cfg::load_env_levels();
    return RUN_ALL_TESTS();
}
