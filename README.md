# Tsunameter
Simulating a Distributed Wireless Sensor Network of Tsunami meters to detect tsunamis. 

## Introduction
This project is to use C and libraries such as OpenMP, PosixThreads and MPI to simulate 
a distributed sensor network of tsunami meters (known as tsunameters) which communicate
with themselves to determine if there is a possible tsunami event. 

To classify as a possible tsunami:
1) A sensor detects the water level above a certain threshold.
2) The sensor communicates with neighbouring sensors.
3) Two or more sensors agree that there is an event (their readings also above the threshold).
4) An alert is sent to the base station. 
5) Base station checks with the satellite altimeter readings to confirm the alert. If there is 
   a match, it is a true alert, else false. Log the results to file.
   
See the TsunameterArchitecture.jpeg to view the architecture of the network. 


## Run
See the instructions in the README.md in the directory. 

## File Structure
The main file is asgn2. There it will call the base station subroutine and sensor routine for
different processes. 


