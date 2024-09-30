# 1. Predecessor

This program is the successor of an previous iteration of the project "Flight Simulator". The program in the repository of the Wiki is the redifined version.

## 1.1 How does the predecessor work?

In flow diagram below is shown how the application works.

![Process diagram predecessor](/images/predecessor/plc-flightsim.png 'Process diagram predecessor')

The application retrieves the current position of the aircraft from MSFS running on the same computer as the application. Meanwhile, via a TCP connection, the application retrieves the current position of the pistons through a PLC. Based on the current position and new position, the application calculates how the pistons should move and sends the calculations to the PLC. When something goes wrong while running the application the pistons are reset to their default position.

> ℹ️ **Tip** <br/> For a more detailed description, please read the documents from past projects. These are avaible in Teams or you can get them through a teacher.

## 1.2 Improvements

### 1.2.1 Design patterns and principles

The code in the previous project does not really have an structure and is sometimes difficult to follow. Sometimes they have used a design pattern, but not constistently.

In the image below you see the structure of the program, visualised as UML diagram. At first glance, it looks like the program is well structured. They even applied an design pattern. But a lot can be improved from this design, we have written out everything in the list below.

![UML diagram predecessor](/images/predecessor/uml.png 'UML diagram predecessor')
_A simplified UML diagram of the predecessor_

#### Unpredictable and strongly connected code

If you look to the UML, you see that the code is split into multiple classes with responsibilities. It is a best ptractice to give each class one responsibility that it is really good at. Rather than giving a class a bunch of responsibilities with the result that the code get's complex over time, hard to read and difficult to maintain. If we look at the UML we already see some duplicate functionalities between classes. If we look directly at the code we see that the classes actually are strongly connected together and just might have been one class.

An example is show in the code snippet below. Here we see the _setCurrentFlightData_ function of the _Main_ class. As you can see the only thing this function does is updating the _currentFlightData_ variable of the _Main_ class. While setting the variable the mutex lock of the _Calculator_ is locked to prevent race conditions. As the _Calculator_ uses the _currentFlightData_ to calculate the positions of the pistons.

```
void Main::setCurrentFlightData(Calculator::FlightData fd)
{
	calculator.m.lock();
	currentFlightData = fd;
	calculator.m.unlock();
}
```

While this looked at a simple task at first, it already becomes very complex and we need to know a lot about the other classes. And it becomes even more complex when we would use the _currentFlightData_ in other classes. Then we would have to lock a mutex lock for each class that depends on _currentFlightData_.

Another example is the code snippet below. Here we have a part of the _getCurrentPositions_ function of the _TCPConnector_. The code checks if we successfully received data over TCP. If we have not it updates a variable defined inside of the _Main_ class. With as result that the program will shut down. So while the responsibility of the _TCPConnector_ is to handle TCP connections, it also decides to shut down the program and knows how the _Main_ class is defined.

```
float* TCPConnector::getCurrentPositions()
{
	...
	if (bytesReceived == 0 || bytesReceived == SOCKET_ERROR)
	{
		Main::running = false;
	}
    ...
}
```

This are two small examples about how all classes and code are strongly coupled. With the result that the code is unpredictable and not testable.

#### Multithreading

Currently only the _currentFlightData_ variable is used on multiple threads in the program. To prevent race conditions a mutex lock is used on this variable. This prevents that the value is not read when updated and updated when read.

Other classes are not designed with multithreading in mind. Like _TCPConnector_, which stores the received piston lengths as class variable. As response it returns a pointer to where the data is stored. When the _getCurrentPositions_ is called another time, the previously received data is overwritten. There is no mutex lock that prevents reading and updating the data at once, thus there is a change that we work with old and new data at once. Currently, the code works fine. But if we would use the _TCPConnector_ at another location, this could lead to problems.

#### Values configured inside class definition

In the code snippet below we see a part of the Calculator class. In the class are a lot of variables defined, like π. There is nothing wrong with defining variables like this, but the code is more flexible and predictable when some variables are injected into a constructor when creating the class at runtime. If we do that for every class, we can configure the whole program within the main function. And when testing, we can inject other values and check if they are used correctly.

```
class Calculator
{
private:
	float pi = 2 * acos(0);

	float legLength = 824.0f;
	float translationX = 0;
	float translationY = 0;
	float translationZ = 935.7456f;

	float defaultXPosition[6]{ 537.69f, 715.23f, 177.53f, -177.53f, -715.25f, -537.69 };
	float defaultYPosition[6]{ 515.43f, 207.94, -723.37f, -723.37f, 207.94f, 515.43f, };

	float defaultXBasePosition[6]{ 177.53f, 715.23f, 537.69f, -537.69f, -715.23f, -177.53f, };
	float defaultYBasePosition[6]{ 723.37, -207.94, -515.43f, -515.43f, -207.94f, 723.37f };
    ...
}

```

<br>

[Chapter 3.](3_Architecture_And_Design) shows how the code will be restructured and how these problems are solved. [Chapter 4.1.2](4_Implementation_And_Tests#412-implementation) shows some examples how the design is implemented.

### 1.2.2 Tests

Currently, there are no unit tests to test the code. It is also not possible to unit test the code. Since no design patterns and principles are applied, it's difficult to test one unit of the application. When designing the successor we keep in mind that it must be possible to test units of the program. And while developing unit tests will be added to test pieces of the application. It's not important to test the whole application, but the essential parts. The essential parts in the case of this program are the parts where the calculations are made. You can read more about testing in [chapter 4.2](4_Implementation_And_Tests#42-test-plan).

### 1.2.3 Handle maximum turn

In Microsoft Flight Simulator, there is no limit how you can move the plane. But since we are limited to the movements of the pistons, we can not make all movements that you can make in the game. Hence, the application must deside which movements are send to de pistons. In the current version of the flight simulator there is a hard limit. The code is shown below.

```
if (nearestRound > 400.0f || nearestRound < 5.0f)
{
	m.unlock();
	throw std::exception();
}
```

When the plane rotates outside of the limit an exeption is thrown. This means that no position is send to the pistons. At first, you may think that this implementation should work OK with not an perfect implementation. But in practice, there are some problems with this implementation.
For example, when you are first within the allowed movement range and then go outside. You keep in the position you are in, before reaching the limit. Then the simulator does not move at all, until you are back inside the allowed range. The scale of the allowed range is evenly spread. It can be the case that you move relatively fast to the limit and then reach the limit and suddenly stand still. When this happens once, it's not ideal. But when you constantly fly at the limit, the simulator starts to malfunction.

When looking at handeling the limits mathematicly, we can compare the current imlementation with the mathematical formula of the step function. Which is displayed in the graph below. With the step function you can easily set hard bounds. But it's not ideal for our use case, as stated earlier.

![Graph of step function](/images/predecessor/step_function.png 'Graph of step function')

What we want is that the limits are approuched slowly, while not going over the set limit. So we want to shift the scale in which the pistons appreach the limits, like in the graph below.

![A graph where the step function is more smooth](/images/predecessor/smoothen_step_function.png 'A graph where the step function is more smooth')

Luckily there is a mathematical formula that is used very often in Computer Science to slowly approach limits called the smooth(er) step function. The graph of this formula is shown in the image below.

![A graph of the smooth step function](/images/predecessor/smooth_step.png 'A graph of the smooth step function')

When applying this formula in our application we don't have the problem we had anymore. When we fly on a limit and go above and under a limit, the difference of the position at the piston is very small. Thus, the system does not malfunction.

The upside of this implementation is also the downside. When a piston moves close to a limit, the movements are scaled down. Which means that if the aircraft experiences turbulence, the turbulence is less than the actual turbulence. For now we will ignore this, but this can be looked at in a future version. See [chapter 6](6_Recommendations) for more recommendations.

### 1.2.4 Visual Studio project

The predecessor was developed as Visual Studio Project. This makes it easy to handle the depenancies via Visual Studio and a lot is handled for you by the IDE. But this means you have to use Visual Studio as your IDE while developing the appliaction. Another option is to use CMake for the successor. CMake, which stands for Cross-platform Make, is an open-source build system that helps manage the build process of software projects. It allows developers to define and configure the build process in a platform-independent manner. CMake generates native build files (such as Makefiles on Unix or Visual Studio project files on Windows) based on a high-level description provided by the developer. This makes it easy to maintain and build projects across different operating systems and build environments. The developer can choose which IDE he wants to use, while the output of the program remains the same. For these reasons, we have choosen to use CMake for the successor.

### 1.2.5 Network protocol

At this moment the program communicates over TCP with the PLC. There are two protocols that can be used for this communication TCP and UDP.

#### UDP

UDP (User Datagram Protocol), has a couple of key characteristics:

-   **Connectionless**: UDP does not establish a connection before sending data and does not guarantee delivery or order of packets. Each packet is treated independently.
-   **Unreliable:** There is no acknowledgment of receipt, and if a packet is lost during transmission, there is no automatic retransmission. Applications using UDP must handle any necessary error checking and recovery.
-   **Low Overhead**: UDP has a smaller header size compared to TCP, resulting in lower overhead. This makes it suitable for applications where low latency and real-time communication are more critical than guaranteed delivery.
-   **Broadcast and Multicast Support**: UDP supports broadcast and multicast communication, making it suitable for scenarios where one-to-many or many-to-many communication is required.

UDP is often used when real-time communication is essential, where the emphasis is on minimizing delay rather than ensuring the delivery of every packet.

#### TCP

TCP (Transmission Control Protocol), has a couple of key characteristics:

-   **Connection-Oriented**: TCP establishes a connection between two devices before data exchange. It ensures that data is delivered in the correct order and without errors. The connection is closed when the data exchange is complete.
-   **Reliability**: TCP ensures reliable and error-checked delivery of data. It uses mechanisms like acknowledgments and retransmissions to guarantee that data sent by one device is correctly received by the other.
-   **Flow Control**: TCP includes flow control mechanisms to manage the rate of data exchange between devices. This prevents one device from overwhelming the other with data, ensuring efficient and stable communication.
-   **Ordered Delivery**: TCP guarantees the order in which data is sent and received. If multiple pieces of data are sent, they will be received in the same order.
-   **Full Duplex Communication**: TCP supports full-duplex communication, allowing data to be sent and received simultaneously between two devices.

#### Which to use?

-   UDP is used for real-time applications, like ours. The flight simulator gets information about the position and has to send the informtaion directly to the PLC. It does not matter if one package get's lost, another package will be send right after the lost package.
-   Over TCP we have a more reliable connection and we know that the packages are dilivered in order. We don't know that for sure when using UDP. However, we can implement an mechanism that checks the order of the packages and ignore packages that are not valid anymore. While still getting the speed of UDP. With TCP it can be the case that one package takes a unexpected long time to send, which keeps up all next packages.
-   The current version is implemented with TCP as protocol. So far there are no problems with the delivery of the data to the PLC.

If we compare the characteristics of both TCP and UDP we can conclude that UDP suits our use case better than TCP. But as stated, the current version works over TCP. When we would switch to UDP we have to update both the flightsimulator program and the PLC, which takes a lot of time.
We choose to keep using TCP for now, but when designing the pplication we keep in mind that TCP must be easily replacable.
