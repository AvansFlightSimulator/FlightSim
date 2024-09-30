# 5. Installation Guide

## 5.1 Prerequisites

-   IDE

    -   [Visual Studio Code](https://code.visualstudio.com) (recommended)
    -   [Visual Studio](https://visualstudio.microsoft.com)
    -   Other IDE that supports C++ and CMake

-   [CMake](https://cmake.org/download/)

    -   Visual Studio Code (recommended)

        -   [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
        -   [C/C++ Themes](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-themes)
        -   [CMake](https://marketplace.visualstudio.com/items?itemName=twxs.cmake)
        -   [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
        -   More information at the [official documentation](https://code.visualstudio.com/docs/cpp/introvideos-cpp).

    -   Visual Studio
        -   Check the [official Microsft documentation](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170) that descibes how to use CMake C++ projects in Visual Studio.

-   Git installed and logged in to account that has permissions to clone the project
-   [ Microsoft Flight Simulator](https://www.flightsimulator.com)

## 5.2 Installation

### 5.2.1 Clone the project

Clone the project via the terminal (or do it via the UI in VS Code)

```
cd folder/to/clone/project_in
git clone git@gitlab.com:AvansInformatica/flight-simulator/flight-simulator.git
```

If you already have cloned the project, click "Open..." and select the location where you have downloaded the project to. If not cloned yet, click "Clone Git Repository...". And then paste `git@gitlab.com:AvansInformatica/flight-simulator/flight-simulator.git` into the text field.

![vs-studio-opening.png](/images/installation/vs-studio-opening.png 'vs studio code start screen')

Download the project dependancies by running the following command in the terminal

```
git submodule update --init --recursive
```

### 5.2.2 Build and run the project

Once you have downloaded and opened the project your screen should look similar to the screen shown in the image below.

![vs-studio-opening.png](/images/installation/vs-studio-project-opened.png 'vs studio code project opened')

#### Build the project

Click the build button in the blue bar at the bottom of the window to build the project (bordered in the image below). The build project can be found via the explorer in the `/build` folder. If there is no output, there was probably an error. The logs are automaticly shown in the "output" screen, also displayed in the image below.

![vs-studio-build.png](/images/installation/vs-studio-build.png 'vs studio code project build')

#### Run the project

To build and run the project click the ▶ button in the blue bar at the bottom of the window. After clicking this button the project gets build and the project will launch in the terminal. Like shown in the image below.

![vs-studio-build-run.png](/images/installation/vs-studio-build-run.png 'vs studio code project build and run')

To quit the running program, click inside the terminal window and type `^C`.

> ⚠️ **Caution** <br> The project is configured with CMake, but this setup is not yet complete. Currently the code that automaticly imports the SimConnect SDK that is used to connect to MSFS is commented out and does not work yet.
