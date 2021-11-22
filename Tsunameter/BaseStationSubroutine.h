#ifndef BASESTATIONSUBROUTINE_H
#define BASESTATIONSUBROUTINE_H

//Function for the base station
//Arguments: water threshold to send alert, comm handler for the 2d grid, comm handle for the world. number of sensors from user input
//Message tags for sendAlertBase
void baseStationSubroutine(float waterThreshold, float maxWaterHeight, int nbaseIters, int nrows, int ncols, MPI_Comm commWorld, int sendAlertBaseTag, int sendTerminationTag, int numSensors, float heightTolerance, float timeTolerance);

#endif
