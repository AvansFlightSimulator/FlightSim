# 4. Realisation and tests

## 4.1 Realisation

### 4.1.1 Folder structure

```
.
├── build
│   ├── CMakeFiles
│
├── external
│   ├── googletest
│   └── nlohmann-json
│
├── include
├── main
├── src
└── tests
```

#### ./build

This folder is the build output folder, when the program compiles all files will be stored here.

#### ./external

The external folder contains all dependancies. Dependancies are stored here as git submodules and imported via CMake. This way it's easy to upgrade to a new version of an dependancy and install all dependancies.

#### ./include

The include folder contains all header files of the library.

#### ./main

The main folder contains the main.cpp file where the main function is defined to run the excecutable.

#### ./src

The src folder contains all .cpp files of the library where the defined code of the header files are implemented.

#### ./tests

The tests folder contains a project that tests the code of the library.

### 4.1.2 Implementation

The code has been implemented according to the design, the established design patterns and design principles have been applied and code has been realized as shown in the UML diagrams.

<br>

In the code snippet below we connect to input and output strategies like explained in chapter 3.2.1. When an error occurs they will call the given _handleError_ method and the program will be shut down gracefully.

```
// connect
auto handleError = [this](bool ok) {
    this->dispose();
};
inputStrategy->connect(handleError);
outputStrategy->connect(handleError);
```

[_Code snippet: setting error handler when starting program_](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/blob/main/src/controller.cpp?ref_type=heads#L15)

In the code snippet below we see the output thread. At first a function is declared that handles new flightdata, we made a function of this to remove duplicate code. We handle the flight data when running the program, but also when the program is shut down. Then we have to move the simulator back to it's initial position.

Also, nothing is really done by the code defined here. All tasks are done by classes which are injected in the [main function](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/blob/main/main/main.cpp?ref_type=heads#L11) of the program. Hence, the SOLID principle is applied well here. The only thing that the thread really handles is when each task is called and with which data. It also handles problems that can occur with multithreading. The _mostRecentFlightData_ is set in the input thread and can be updated while calculating the positions of the pistons. To prevent race conditions we use a mutex lock. We lock the mutex when using the data, so it can not be updated, and unlock the data when we are done.

```
// start output thread
std::thread output([this] {
    auto handleFlightData = [this](FlightData* fd){
        // get current positions
        float positions[6];
        this->outputStrategy->getCurrentPositions(positions);

        // calculate speed and movements
        fdm.lock();
        Calculator::CalculationData data = calculator->calculatePistons(fd, positions);
        fdm.unlock();

        // send update
        outputStrategy->updatePositions(data);
    };

    while (isRunning)
    {
        handleFlightData(mostRecentFlightData);
    }

    // set to default position
    FlightData dflt = FlightData();
    handleFlightData(&dflt);
});

```

[_Code snippet: reset position to default position when program quits_](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/blob/main/src/controller.cpp?ref_type=heads#L24)

We can not get a race condition with the other data, even not when we use the strategies on other threads (which is not the case right now). The _getCurrentPositions_ stores the data in the _positions_ variable created within the scope of this thread.

When buffering data, like when getting the current piston positions, we store the buffer within the scope of the call and don't buffer within the scope of the class. This prevents race conditions when functions are called on multiple threads simultaneously. Also when called simultaneously they can run parallel, we don't have the use a mutex lock on the buffer and run them in sequence.

Also, here you see that when an error occurs we call the given _onError_ function as stated earlier in this chapter.

```
void TCPOutputStrategy::getCurrentPositions(float positions[6]) {
    // get data from the client over the network
    char buf[4096];
    int nBytes = client->getData(buf);
    if (nBytes == 0) {
        onError(false);
    }

    try
    {
        // map buffer to json
        json d = json::parse(buf, 0, nBytes);

        auto l = d["currentPositions"];

        // set positions
        int pos = 0;
        for (auto i = l.begin(); i != l.end(); ++i)
        {
            positions[pos] = i.value();
            pos++;
        }
    }
    catch(const std::exception& e)
    {
        onError(false);
    }
}

```

[_Code snippet: get positions of pistons from the network client_](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/blob/main/src/tcp_output_strategy.cpp?ref_type=heads#L11)

All classes are configured in the main function of the program. No abstract classes or concrete classes define any configurations about the working of the program. Like the minimum and maximum position of a piston or the default positions of the simulator. As shown in the code snippet below.

This makes the code easy to configure and easy to understand and test. The configuration now happens at one place and we don't have so called "magic numbers" in our code.

```
// setup calculator

SmoothStepPistonStrategy pistonStrategy = SmoothStepPistonStrategy(5.0f, 400.0f);

float x[6] = {537.69f, 715.23f, 177.53f, -177.53f, -715.25f, -537.69f};
float y[6] = {515.43f, 207.94f, -723.37f, -723.37f, 207.94f, 515.43f};
Calculator::Positions defaultPositions(x, y);

float bx[6] = {177.53f, 715.23f, 537.69f, -537.69f, -715.23f, -177.53f};
float by[6] = {723.37f, -207.94f, -515.43f, -515.43f, -207.94f, 723.37f};
Calculator::Positions defaultBasePositions(bx, by);

CalculatorImpl c = CalculatorImpl(824.0f, 0.0f, 935.7456f, defaultPositions, defaultBasePositions, &pistonStrategy);

```

[_Code snippet: the calculator is configured in the main function_](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/blob/main/main/main.cpp?ref_type=heads#L15)

## 4.2 Test plan

## 4.2.1 Scope

The calculations that are made should be tested to see if they are made correctly. These should be tested separately.

The established requirements should be tested, to verify that the expectations for this project have been met.

## 4.2.2 Testers

The computer tests the code by running automated tests.

The supervisors will review the code and test if the applications works like expected.

## 4.2.3 Test planning

In the last week the system will be tested together with a supervisor, to see if the system works correctly. He will also review the code.

Automated tests are performed when making a commit.

## 4.2.4 Levels of testing

As defined in the test scope, all calculations are tested to see if they are done correctly. The calculations are tested by means of unit tests. This involves testing a specific component and without involving dependencies. Should a piece of code be dependent, a mock is injected. The unit tests are written while the code is being developed and not afterwards.

Given the project timeline and knowledge within the team, it was decided not to implement other automated test levels yet.

## 4.2.5 Testing techniques

For developing the unit tests, the widely used package "googletest" is used.

> ⚠️ **Note** <br> It's recommended to test more than is done during this project. Please see [chapter 6.2.3](6_Recommendations#623-add-more-tests) for more information.
