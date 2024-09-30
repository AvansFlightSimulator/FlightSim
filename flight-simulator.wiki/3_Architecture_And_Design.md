# 3. Architecture and design

## 3.1 Design patterns & principles

Multiple design principles will be applied when designing and writing the application, such as the SOLID-principle. Which is an acronym for:

-   **Single responsibility principle**: each class should have one responsibility or job
-   **Open-closed principle**: Software entities (such as classes, modules, functions) should be open for extension but closed for modification. This encourages the use of abstraction and polymorphism, allowing new functionality to be added without altering existing code.
-   **Liskov Substitution principe**: Objects of a superclass should be replaceable with objects of a subclass without disrupting the behavior of the program
-   **Interface Segregation principle**: A client should not be forced to depend on interfaces it does not use.
-   **Dependency Inversion principle**: High-level modules should not depend on low-level modules; both should depend on abstractions.

Applying these principles makes the software easier to maintain, test, read and replace. Which is very importend, as stated in the requirements.

![UML diagram flightsimulator](/images/architecture_and_design/uml_flightsim.png 'UML diagram flightsimulator')

The UML diagram of the flight simulator is given in the image above.

As you can see in the UML the SOLID principles are applied, each class has an interface/abstraction. And no concrete implementation references to another concrete implementaion. Instead, it references to the abstract version of that class. Each class has a single responsibility and can easily be replaced.

Take, for example, the _MaxPistonStrategy_. Here we apply the SOLID principles by referencing the to the abstract class, but at runtime we inject the concrete _SmoothStepMaxPistonStrategy_. The strategy also has only one responsibility; calculate the value within the limits. The _SmoothStepMaxPistonStrategy_ could be easily replaced by another _MaxPistonStrategy_, without changing the behaviour of the program, the only thing that changes is how the limits are handled. We also apply design pattern here, called the strategy pattern.

> The Strategy Design Pattern is a behavioral design pattern that defines a family of algorithms, encapsulates each algorithm, and makes them interchangeable. It allows a client to choose an algorithm from a family of algorithms at runtime, without modifying the client's code. The pattern involves defining a set of algorithms, encapsulating each one in a separate class, and making these classes interchangeable.

The predecessor contained a lot of duplicate code, for example each time degrees had to be mapped to radians this was done by writing out the whole formula. Now, we define functions to handle cases like this. This removes duplicate code, while making the code simpeler, more readable and reduces the chance of errors. This is often called KISS (Keep It Simple, Stupid!), removing unnecessary complexity.

The predecessor had a global accessible bool variable avaible that was set to false if an error occured and the program should be shut down. This goes agains the design principles we defined and also makes the code very complex, difficult to predict and difficult to test.
Instead when initializing a class that can throw an error at runtime, we configure an _onError_ callback. When an error occurs within the class, the callback is called. This way, the class is not responsible for how to handle the error and does not know how the error will be handled. This is up to to the class that is responsible for running the program. Which is in this case the _Controller_.

## 3.2 Flow

### 3.2.1 Main thread

![main flow diagram](/images/architecture_and_design/main_flow_diagram.png 'main flow diagram')

1. The program is started and all classes are created and injected, if the application is running on windows we will use the _MSFSInputStrategy_, if not we will simulate the input with _CSVInputStrategy_.

2. We connect to the input. When the _MSFSInputStrategy_ is injected, we check if the "Microsoft Flight Simulator" is running. If it is, we connect to the program and setup the _onError_ handler. If not, we will wait x amount of time and check again. Until the program is active. If a time limit is reached, we close the program.

3. We connect to the output and setup the _onError_.

4. We start a thread for both tasks, so that both tasks can be done simultaneously. This way we have a minimum response time and the most recent data to calculate the current position.

5. When an error occurs on one of the threads, we close both threads. Each thread can handle the shut down as they want, as explained in the flow diagrams below.

6. We close the program

### 3.2.2 Output thread

![output flow diagram](/images/architecture_and_design/output_flow_diagram.png 'output flow diagram')

1. We get the current position of the pistons via the _OutputStrategy_, hence over the network from the PLC

2. Based on the most recent flightdata we have and the current position of the pistons we calculate how the pistons should move. We do this per piston, after calculating we apply the smooth step formula like stated in the requirements. After calculating each position per piston, we calculate at what speed each piston has to move. As they have to move simultaneously and they don't all have to move the same distance.

3. We send the calculated data via the _OutputStrategy_, hence we send the new positions and speeds to the PLC.

### 3.2.3 Input thread

![input flow diagram](/images/architecture_and_design/input_flow_diagram.png 'input flow diagram')

1. We get the current FlightData via the _InputStrategy_, hence we get the data from Microsoft Flight Simulator when running on windows. If not running on Windows, we simulate movements.
