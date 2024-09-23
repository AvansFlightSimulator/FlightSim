#include <gtest/gtest.h>

#include "../include/tcp_client.h"
#include "../include/tcp_output_strategy.h"

class MockClient : public NetworkClient
{
public:
    MockClient() {

    };

    bool initialize(char* addr, int port) {
        return true;
    }

    int getData(char* data) {
        strcpy(data, R"({"currentPositions": [1.9999, 2.567]})");
        return 1;
    }

    bool sendData(std::string data) {
        return true;
    }

    void dispose() {

    }
};

TEST(test, test) {
    auto v = MockClient();
    TCPOutputStrategy strategy = TCPOutputStrategy(&v);
    float positions[6];
    strategy.getCurrentPositions(positions);

    EXPECT_FLOAT_EQ(positions[0], 1.9999f) << "value -> " << positions[0] << " expected 1.9999";
    EXPECT_FLOAT_EQ(positions[1], 2.567f) << "value -> " << positions[0] << " expected 2.567";
};