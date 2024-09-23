#ifndef __MFS_INPUT_STRATEGY_H__
#define __MFS_INPUT_STRATEGY_H__

#include "input_strategy.h"

#ifdef _WIN32
#include "Windows.h"
#include "SimConnect.h"

class MFSInputStrategy : public InputStrategy {
    private:
        int maxWaitInSec; 
        int waitBetweenTriesInSec;
        std::function<void(bool)> onError;
        HANDLE simConnect = NULL;

        enum DATA_DEFINE_ID
        {
            DEF1
        };

        enum DATA_REQUEST_ID
        {
            REQ1
        };

        
    public:
        MFSInputStrategy(int maxWaitInSec, int waitBetweenTriesInSec) {
            this->maxWaitInSec = maxWaitInSec;
            this->waitBetweenTriesInSec = waitBetweenTriesInSec;
        };

        bool isOK() override;
        bool connect(std::function<void(bool)>) override;
        FlightData getInput() override;
};

#endif

#endif