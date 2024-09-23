#ifndef __FLIGHT_DATA_H__
#define __FLIGHT_DATA_H__

struct FlightData {
    float pitch, bank, rudder, transX, transY;

    FlightData(float pitch = 0.0f, float bank = 0.0f, float rudder = 0.0f, float transX = 0.0f, float transY = 0.0f) {
        this->pitch = pitch;
        this->bank = bank;
        this->rudder = rudder;
        this->transX = transX;
        this->transY = transY;
    }
};

#endif