#ifndef SENSORSUBROUTINE_H
#define SENSORSUBROUTINE_H

//Function for the each sensor subroutine
//Arguments: rank of current process, the process' coordinates, the neighbours' ranks (may or may not exist),  min water height 
//that can be generated, max water height, water threshold to send alert, comm handler for the 2d grid, comm handle for the world.
//Message tags for different types of messages and water height tolerance for similarity. 
void sensorRoutine(int rank, int coord[2], int neighbours[4], float minWaterHeight, float maxWaterHeight, float waterThreshold, MPI_Comm comm2D, MPI_Comm commWorld, int sendRequestTag, int sendAvgTag, int sendAlertBaseTag, int sendTerminationTag, float heightTolerance);

#endif
