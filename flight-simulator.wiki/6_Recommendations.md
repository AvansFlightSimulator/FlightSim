# 6. Recommendations

## 6.1 TODO 📝

### 6.1.1 Update CMake script to import SDK

The project is set up with CMake, which handles how to bundle all files and dependancies. Unfortunately, we get an error when importing the SimConnect SDK. The scripts that import the SDK are commented out, thus it is possible to build the project. But it currently not possible to use the MSFSInputStrategy to get the information from MSFS. Although, the code works.

## 6.2 Recommendations

### 6.2.1 Network protocol

As stated in [chapter 1.2.5](1_Predecessor#125-network-protocol) it would be better to use UDP than TCP for this project. But this is not yet implemented, to improve the project i would be possible to switch from TCP to UDP. But than the PLC program needs to be updated too.

### 6.2.2 Turbulence

As stated in [chapter 1.2.3](1_Predecessor#123-handle-maximum-turn) we now handle the limits of the pistons via the smooth step function. With the result that the turbulence also becomes smaller as you reach a limit. As far as we have found there is no way to query the turbulence from MSFS, so if the turbulence must remain the same over the scale, a method must be found on how to calculate it.

### 6.2.3 Testing

At the moment, a few unit tests have been developed that check the calculations in the code. furthermore, other parts in the application are not yet tested automatically, these have been tested manually. but for further development, we recommend writing tests for these first, to prevent parts from suddenly no longer working.

Also, no mock framework is currently used to abstract dependencies. However, a good framework to use for this has already been looked at. we recommend looking at "gMock", for mocking dependencies. Given lack of time and experience within the team, this has not yet been implemented.

Further, we recommend adding static code analytics and a automated testing pipeline to the project.
