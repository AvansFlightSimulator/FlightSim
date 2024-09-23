#include "../include/cout_output_strategy.h"
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void CoutOutputStrategy::getCurrentPositions(float positions[6]) {
    std::cout << "[DEBUG]   CoutOutputStrategy::getCurrentPositions\n";

    std::this_thread::sleep_for(std::chrono::seconds(1));

    char buf[4096] =  R"({"currentPositions": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]})";
    json d = json::parse(buf, 0, sizeof(buf));
    auto l = d["currentPositions"];
    int index = 0;

    for (auto i = l.begin(); i != l.end(); ++i)
    {
        positions[index] = i.value();
        index++;
    }
}

bool CoutOutputStrategy::connect(std::function<void(bool)>) {
    return true;
}

bool CoutOutputStrategy::updatePositions(Calculator::CalculationData data) {
    std::cout << "[DEBUG]   CoutOutputStrategy::updatePositions -> ";

    int index = 0;
    for(auto i : data.lengths) {
        positions[index] = i;
        std::cout <<  positions[index] << " ";
        i++;
    }
    std::cout << "\n";
    return true;
}

bool CoutOutputStrategy::isOK() {
    return true;
}