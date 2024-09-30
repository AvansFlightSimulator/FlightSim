# 2. Requirements

In this chapter we discuss the requirements of the successor and other requirements are determined. The requirements are based on the predecessor and requirements given by the project supervisors.

# 2.1 Functionals

| ID  | Requirement                                                            | Acceptence                                                                                                          |
| --- | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| F1  | Pistons may not move beyond defined values                             | The pistons should not suddenly stop moving, but should slowly reach the limit                                      |
| F2  | When the system stops, the pistons must return to the default position | The platform should be horizontal when stopped, this code must also happen when an error occured for safety reasons |
|     |
| F3  | The movements from MSFS are simulated by the pistons                   | This must happen in real-time (ms delay)                                                                            |
|     |

# 2.2 Non-functionals

| ID  | Requirement                                                                                                      |
| --- | ---------------------------------------------------------------------------------------------------------------- |
| NF1 | Code must be avaible via git (GitLab)                                                                            |
| NF2 | Code must be documented in the Wiki (GitLab)                                                                     |
| NF3 | Comments must be added to code so that it is clear to other developers what the code does                        |
| NF4 | An installation guide must be added to the Wiki (GitLab)                                                         |
| NF5 | The architecture and design of the code must be added to the Wiki , including argumentation about design choices |
| NF6 | The code must be readable by other developers                                                                    |
| NF7 | A test plan should be added to the Wiki (GitLab)                                                                 |
| NF8 | Recommendations and todo's should be added to the Wiki (GitLab)                                                  |

# 2.3 Conditions

| ID  | Requirement                                                    |
| --- | -------------------------------------------------------------- |
| C1  | The application must run on the PC in the workshop             |
| C2  | The application must send the data over the network to the PLC |
| C3  | The application must be written in C++                         |

# 2.4 Extra

This are some extra 's added by the developer of the application:

-   Read data from .csv file to simulate movements (for demonstration or development/tests)
-   Log data to console instead of sending to PLC (for demonstration or development/tests)
-   Support Linux/macOS besides Windows (for development)
