#include "../include/csv_input_strategy.h"

bool CSVInputStrategy::isOK() {
	return true;
}

bool CSVInputStrategy::connect(std::function<void(bool)> onError) {
    getline(s, line);
    return true;
}

FlightData CSVInputStrategy::getInput() {
    if (getline(s, line)) {
        std::stringstream l (line); 
        std::vector<float> v;

        while(getline(l, word, ',')) {
            v.push_back(std::stod(word));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(msPerRow));
        // std::cout << "[DEBUG]    FlightData(" << v.at(0) << ", "<<  v.at(1) << ", "<<  v.at(2) << ", "<<  v.at(3) << ", "<<  v.at(4) << ")\n";

        return FlightData(v.at(0), v.at(1) * -1, v.at(2) * -1, v.at(3), v.at(4));
    } else {
        std::cout << "[WARN]    no next line, returning FlightData()\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(msPerRow));
    }
    
	return FlightData();
}

CSVInputStrategy::~CSVInputStrategy() {
    s.close();
}