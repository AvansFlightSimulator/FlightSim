#include <gtest/gtest.h>
#include "../include/smooth_step_piston_strategy.h"

TEST(flight_simulator_tests, max_piston_strategy_test_max){
    SmoothStepPistonStrategy strategy(5.0, 400.0);
    float expected = 400.0f;
    float res = strategy.getValue(400.0f);

    EXPECT_FLOAT_EQ(res, expected) << "failed with value: " << res << ", expected: " << expected;
};

TEST(flight_simulator_tests, max_piston_strategy_test_min){
    SmoothStepPistonStrategy strategy(5.0, 400.0);
    float expected = 5.0f;
    float res = strategy.getValue(5.0f);

    EXPECT_FLOAT_EQ(res, expected) << "failed with value: " << res << ", expected: " << expected;
};

TEST(flight_simulator_tests, max_piston_strategy_test_below){
    SmoothStepPistonStrategy strategy(5.0, 400.0);
    float expected = 5.0f;
    float res = strategy.getValue(0.0f);

    EXPECT_FLOAT_EQ(res, expected) << "failed with value: " << res << ", expected: " << expected;
};

TEST(flight_simulator_tests, max_piston_strategy_test_above){
    SmoothStepPistonStrategy strategy(5.0, 400.0);
    float expected = 400.0f;
    float res = strategy.getValue(450.0f);

    EXPECT_FLOAT_EQ(res, expected) << "failed with value: " << res << ", expected: " << expected;
};

TEST(flight_simulator_tests, max_piston_strategy_test_middle){
    SmoothStepPistonStrategy strategy(5.0, 400.0);
    float expected = 202.5f;
    float res = strategy.getValue(202.5f);

    EXPECT_FLOAT_EQ(res, expected) << "failed with value: " << res << ", expected: " << expected;
};
