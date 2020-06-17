# COMP 304 Project 2: Air Traffic Control

## Student Name: Furkan Sahbaz

## Student ID: 60124

A C++ program to generate an air traffic simulation using the pthread mutexes and condition variables to prevent deadlocks, starvation, and any possible race condition was was implemented for this project. All of the parts of the project work according to the project description.

The program takes the simulation time, probability if planes arriving each second, and after how many seconds the output will be printed to the console from the user. An example execution would be as follows:

```bash
make atc
./atc.out -s 60 -p 0.5 -n 10
```

which will run the simulation for 60 seconds, with 50\% chance of planes arriving each second; and after the 10th second the situation of the waiting queues will be printed to the console.

## Main Program

Initially, the command line arguments are parsed, and the necessary mutexes and condition variables are initiated. After the initiations are complete, a set of threads that form the first 2 planes (1 arriving and 1 departing) waiting before the simulation starts, and the control tower that manages the planes' arrival and departure by allocating the runway to the prioritized plane are created. Afterwards,the simulation is started to last for the given amount of simulation time. Within the simulation loop, arriving and departing planes are generated each second depending on the random probability value obtained in each iteration (however, a plane requiring an emergency landing arrives every 40 seconds), and the content of the waiting queues are printed according to the comparison between the elapsed time and the required time given by the user.

## Control Tower

The control tower waits for the first plane to make contact with it before starting the simulation by waiting for the on_runway mutex, which along with the runway_lock mutex, secures the variable that keeps the number of planes that are currently in contact with the tower. After the simulation starts and the log file along with the counter to prevent starvation of any thread is initiated, the control tower first checks whether there's a plane waiting for an emergency landing. If there's none, then it checks if the depart queue is not empty and the counter value is less than 4, along with the conditions of more than 5 planes waiting to depart pr the arrival queue being empty. If none of these conditions hold, it proceeds to operating on the arrival queue. An important point here is the counter value that is checked before handling a departure, as the landing planes before will face starvation if it is not present, since the accumulation of departing planes before any of them is granted to depart may take a long time to exhaust. Checking whether the arrival queue is empty in this condition is critical as the arrival queue may be empty when proceeding to that section, and popping an empty queue may cause additional trouble. For all types of queues, the same operation is performed to handle their request: after the locks are complete, the number of plains currently in contact are reduced for reference, the plane with permission is removed from the corresponding queue, and the ID of the plane with permission to use the runway is updated. After the operations are complete, all planes waiting for the t_lock are notified, however only the one with the updated ID can acquire the lock and exit its thread. The log file is updated in every turn, where each operation (landing or departing a plane) takes 2 seconds. After the simulation is complete on the tower's part, it closes the log file and exits.

## Arriving and Departing Planes

Each time an arriving plane or a departing plane thread is created, a plane is generated according to the given status (and also according to the elapsed time, in the case of an arriving plane) with the generate_plane function. This function initiates a Plane struct and its necessary fields, notifies the tower if the plane being generated is the first plane with the on_runway signal, while acting under the runway_lock mutex, and returns the ID of the newly-generated plane. On the arriving/departing plane thread side, the thread waits for a signal from the tower with the t_lock mutex and t_cond condition variable until the ID of the plane with the grant to use the runway equals that of the thread at hand's. When the corresponding plane picks up the signal after the tower broadcasts, its thread exits.

## Program Outputs

2 types of outputs are provided by the program:

### Printing to the console

After enough time has elapsed, the program prints the contents of the waiting and departing queues to the console. An example screenshot from the console ouput would be as follows:

```bash
Starting simulation.
Planes in the air at 39s:
| 60 || 64 || 68 || 72 |
Planes on the ground at 39s.
| 23 || 25 || 31 || 35 || 39 || 41 || 47 || 51 || 55 || 59 || 63 || 67 || 71 || 75 |
Planes in the air at 40s:
| 76 || 60 || 64 || 68 || 72 |
Planes on the ground at 40s.
| 23 || 25 || 31 || 35 || 39 || 41 || 47 || 51 || 55 || 59 || 63 || 67 || 71 || 75 || 79 |
Planes in the air at 41s:
| 60 || 64 || 68 || 72 |
Planes on the ground at 41s.
| 23 || 25 || 31 || 35 || 39 || 41 || 47 || 51 || 55 || 59 || 63 || 67 || 71 || 75 || 79 |
```

### Logging to the file

Each time the tower completes its operation, information about the plane that was handled, containing the ID of the plane, its status, request time,the time when its runway usage ends, and its turnaround time (runway time - reques time) is entered to the log file. An example screenshot from the log file would be as follows:

```bash
Plane ID   	Status   	Request Time   	Runway Time   	Turnaround Time
_______________________________________________________________________
  52 		  L 		    28 		        8 		        10
  56 		  L 		    30 		        40 		        10
  76 		  E 		    40 		        42 		        2
  23 		  D 		    11 		        44 		        33
```
