#include <gtest/gtest.h>
#include "../include/flight_data.h"
#include "../include/math_func.h"

TEST(flight_simulator_tests, degToRad){
    float expected = 0.01745329238474369;
    float res = degToRad(1);

    EXPECT_TRUE(res == expected) << "failed with value: " << res << ", expected: " << expected;
};

TEST(flight_simulator_tests, smoothstep){
    float expected = 0.5;
    float res = smoothstep(0, 1, 0.5);

    EXPECT_TRUE(res == expected) << "failed with value: " << res << ", expected: " << expected;
};
