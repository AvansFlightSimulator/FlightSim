#include "../include/calculations.h"
#include "../include/math_func.h"
#include <iostream>

Calculator::CalculationData CalculatorImpl::calculatePistons(FlightData* fd, float* oldPistonLengths) {
    float bankRad = degToRad(fd->bank), picthRad = degToRad(fd->pitch), yawRad = degToRad(fd->rudder);
    float rollC = cosf(bankRad), picthC = cosf(bankRad), yawC = cosf(yawRad);
    float rollS = sinf(bankRad), picthS = sinf(picthRad), yawS = sinf(yawRad);

    float res = 0;
    float positionsX[6], positionsY[6], positionsZ[6];

    Calculator::CalculationData data;

    for (int i=0; i<numOfPistons; i++) {
        // calculate length of piston based on flightdata
        positionsX[i] = fd->transX + yawC * rollC * defaultPositions.x[i] + (-yawS * picthC + yawC * rollS * picthS) * defaultPositions.y[i];
        positionsY[i] = fd->transY + yawS * rollC * defaultPositions.x[i] + defaultPositions.y[i] * (yawC * picthC + yawS * picthS * rollS);
		positionsZ[i] = translationZ - rollS * defaultPositions.x[i] + rollC * picthS * defaultPositions.y[i];

        res = roundf((sqrtf(powf(positionsX[i] - defaultBasePositions.x[i], 2) + powf(positionsY[i] - defaultBasePositions.y[i], 2) + powf(positionsZ[i], 2)) - legLength) * 100.0f) / 100.0f;

        // round strategy
        res = pistonStrategy->getValue(res);

        // set result at index i
        data.lengths[i] = res;
    }

    float pistonDeltaLengths[6];

    // calculate the total time needed for the maximum movement needed at the max velocity
    float maxPistonDeltaLength = 0.0f, pistonMovementTime = 0;
    for (int i = 0; i < numOfPistons; i++)
	{
		pistonDeltaLengths[i] = abs(oldPistonLengths[i] - data.lengths[i]);

		if (pistonDeltaLengths[i] > maxPistonDeltaLength)
		{
			maxPistonDeltaLength = pistonDeltaLengths[i];
		}

		pistonMovementTime = maxPistonDeltaLength / maxVelocity;
	}

    // calculate the speed per piston that it needs to move at to stop at the same time
    for (int i = 0; i < numOfPistons; i++)
	{
		data.speeds[i] = roundf((pistonDeltaLengths[i] / pistonMovementTime) * 100.0f) / 100.0f;
	}

    return data;
}