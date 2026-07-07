#pragma once

#include <cmath>
#include <cstdlib>
#include <iostream>

#define FT_EXPECT_TRUE(expr) do { if (!(expr)) { std::cerr << "expected true: " #expr << " at " << __FILE__ << ":" << __LINE__ << "\n"; std::exit(1); } } while (false)
#define FT_EXPECT_NEAR(actual, expected, tolerance) do { const auto a_ = (actual); const auto e_ = (expected); const auto t_ = (tolerance); if (std::abs(a_ - e_) > t_) { std::cerr << "expected " #actual " near " << e_ << ", got " << a_ << " tolerance " << t_ << " at " << __FILE__ << ":" << __LINE__ << "\n"; std::exit(1); } } while (false)

inline float centsBetween(float actualHz, float expectedHz)
{
    return 1200.0f * std::log2(actualHz / expectedHz);
}
