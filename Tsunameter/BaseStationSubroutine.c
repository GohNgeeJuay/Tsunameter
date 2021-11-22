#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "BaseStationSubroutine.h"
#include "HelperFunctions.h"


#define NUM_THREADS 2
#define SATELITE_SIZE 100

// thread mutex and cond vars etc
pthread_mutex_t g_Mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_Cond = PTHREAD_COND_INITIALIZER; // may not be necessary

int userSentinelValue = 1; 
int baseSentinelValue = 1;

// Thread function which checks whether the user has given termination signal in the file use.txt
void *userEnd(void *pArg) {
    // check value until user changes to 0 or base ends - need global base end and user sent
    // if user changes the value -> update global var and exit
    // if base end due to iterations -> exit
    while (userSentinelValue == 1 & baseSentinelValue == 1) {
        sleep(1);
        FILE* file = fopen("use.txt", "r");
        fscanf(file, "%d", &userSentinelValue);
        fclose(file); 
    }
    return NULL;
}

int nextIndex = 0;
float waterT;
float maxHeight;
int sensorRows, sensorCols;

//Satellite values
struct satVal{
    int sat_coord1;
    int sat_coord2;
    float satHeight;
    time_t satTime;
};

//Alert from sensors
struct alert{
    int sensorRank;
    int sensor_coord1;
    int sensor_coord2;
    float sensorHeight;
    int neighRank[4];
    int neigh_coord1[4];
    int neigh_coord2[4];
    float neighHeight[4];
    int similarCount;
    time_t sensorTime;
    int numMsgs;
};

struct satVal satReadings[SATELITE_SIZE]; //global array which is populated with satellite readings
int satReadCount = 0;
double totalCommTime = 0.0;   //total communication time for base station and sensors throughout program

// satelite t function
void *sateliteAltimeter(void *pArg) {
    // while not terminated, get next index, generate new reading, coord and time
    // mutex lock, add to arrays, unlock and signal -figure order + sleep
    
    float thisReading;
    int coord1, coord2;
    srand(time(NULL));
    while (userSentinelValue == 1 & baseSentinelValue == 1) {
        thisReading = randomFloatWaterLevel(waterT, maxHeight); //generate new random water level
        coord1 = rand()%sensorRows;   //new coordinates
        coord2 = rand()%sensorCols;
        
        pthread_mutex_lock(&g_Mutex);

        satReadings[nextIndex].satHeight = thisReading;
        satReadings[nextIndex].sat_coord1 = coord1;
        satReadings[nextIndex].sat_coord2 = coord2;
        time(&satReadings[nextIndex].satTime);

        pthread_mutex_unlock(&g_Mutex);

        pthread_cond_signal(&g_Cond);
        
        nextIndex = (nextIndex+1)%SATELITE_SIZE;
        satReadCount++;
        sleep(1);
    }

    return NULL;
}



//Function to log info to file
void writeToLog(int iteration, int senRank, int senC1, int senC2, int satC1, int satC2, time_t senTime, time_t satTime, float senHeight, float satHeight, int neiMatchCount, int neiRank[], int neiC1[], int neiC2[], float neiHeight[], int alertType, int neiMsgs) {
    double sensorBaseCommTime;
    time_t nowTime;
    nowTime = time(NULL);
    sensorBaseCommTime = fabs(difftime(senTime, nowTime));
    totalCommTime += sensorBaseCommTime;

    FILE* file = fopen("logs.txt", "a");
    fprintf(file, "=====================    NEW RECORD    =====================\n");
    /*
    Content: base iteration, base time, sensor time, match t/f, report node, coord, height (ipv4), same for neighbours, sat time, sat height, sat coord, comm time, total msgs, matching neighbours, tolerances
    */
    fprintf(file, "Iteration: %d\n", iteration);
    fprintf(file, "Logged time: %s\n", ctime(&nowTime));
    fprintf(file, "Alert reported time: %s\n", ctime(&senTime));
    if (alertType == 1)
        fprintf(file, "Alert type: True\n");
    else
        fprintf(file, "Alert type: False\n");

    // fprintf(file, "\nReporting Node\t\tCoord\t\tHeight(m)\t\tIPv4\n");
    fprintf(file, "\nReporting Node\t\tCoord\t\tHeight(m)\n");
    fprintf(file, "%d\t\t\t\t\t(%d, %d)\t\t%.3f\n", senRank, senC1, senC2, senHeight);

    fprintf(file, "\nAdjacent Nodes\t\tCoord\t\tHeight(m)\n");
    for (int i=0; i<4; i++) {
        if (neiRank[i] != -2)
            fprintf(file,"%d\t\t\t\t\t(%d, %d)\t\t%.3f\n", neiRank[i], neiC1[i], neiC2[i], neiHeight[i]);
    }

    if (satC1 != -1) {
        fprintf(file, "\nSatellite altimeter reporting time: %s", ctime(&satTime));
        fprintf(file, "Satellite altimeter reporting height (m): %.3f\n", satHeight);
        fprintf(file, "Satellite altimeter reporting Coord: (%d, %d)\n", satC1, satC2);
    } else 
        fprintf(file, "\nSatellite altimeter record not found for alert sensor location.\n");

    fprintf(file, "\nCommunication Time between sensor and base (seconds): %.3f\n", sensorBaseCommTime);
    fprintf(file, "Total Messages sent between reporting node and base station: %d\n", 1);
    fprintf(file, "Number of adjacent matches to reporting node: %d\n", neiMatchCount);
    fprintf(file, "Total Messages sent between reporting node and its neighbours: %d\n", neiMsgs-1);

    fprintf(file, "============================================================\n");
    fclose(file); 
}

void baseStationSubroutine(float waterThreshold, float maxWaterHeight, int nbaseIters, int nrows, int ncols, MPI_Comm commWorld, int sendAlertBaseTag, int sendTerminationTag, int numSensors, float heightTolerance, float timeTolerance){

    //declare and init local variables
    int baseIterCount = 0;
    waterT = waterThreshold;
    maxHeight = maxWaterHeight;
    sensorRows = nrows;
    sensorCols = ncols;
    int* alertSensorFlags; //alert if sensor sends an alert
    int world_size; 
    MPI_Comm_size(commWorld, &world_size);
    MPI_Request request[numSensors];
	MPI_Status status[numSensors]; //all sensors have a possible chance of sending alert to base station, space for each send

    pthread_t hThread[NUM_THREADS]; // Stores the POSIX thread IDs
	int threadNum[NUM_THREADS]; // Pass a unique thread ID
    // Initialize the mutex & condition variable
	pthread_mutex_init(&g_Mutex, NULL);
	pthread_cond_init(&g_Cond, NULL);
    // Create both threads
	threadNum[0] = 0;
	pthread_create(&hThread[0], NULL, userEnd, &threadNum[0]);
	threadNum[1] = 1;
	pthread_create(&hThread[1], NULL, sateliteAltimeter, &threadNum[1]);
	
	//The initial values for alerts from sensors is 0 (no alerts)
    int sensorTrueAlerts[numSensors];
    int sensorFalseAlerts[numSensors];
    for (int a=0; a<numSensors; a++) {
        sensorTrueAlerts[a] = 0;
        sensorFalseAlerts[a] = 0;
    }
    int totalMsgCount = 0;
    

    // init log file with tolerance for height and time readings
    FILE* file = fopen("logs.txt", "a");

    fprintf(file, "=====================    LOGS    =====================\n");
    fprintf(file, "\nMax. tolerance for all height readings (m): %.1f\n", heightTolerance );
    fprintf(file, "Max. tolerance for all time readings (sec): %.1f\n",timeTolerance);
    fprintf(file, "Sensor grid structure: %d x %d\n\n", nrows, ncols);

    fclose(file); 

    
    //Iterate while user does not provide the sentinel value
    while (userSentinelValue == 1 & baseSentinelValue == 1){
                    
        //listen periodically for the alert from sensors
        sleep(4); //check every 5 seconds (similar to sensors interval
        

        //Array to keep track if any sensors have a message
        alertSensorFlags = (int*)calloc(numSensors, sizeof(int));

        //Check if any sensors have send an alert -> NOTE: thread per probe
        for (int i = 1; i <= numSensors; i++){
            MPI_Iprobe(i,sendAlertBaseTag,commWorld,&alertSensorFlags[i], &status[i]);
            
            if (alertSensorFlags[i]!= 0){ 
                int alertType = 0;
                struct alert sensorAlert;
                struct satVal satReadMatch;
                satReadMatch.sat_coord1 = -1;

                MPI_Recv(&sensorAlert, sizeof(struct alert), MPI_CHAR, i, sendAlertBaseTag, commWorld, &status[i]);

                totalMsgCount += sensorAlert.numMsgs;
                // satelite check
                int satEnd = SATELITE_SIZE;
                if (satReadCount < SATELITE_SIZE)
                    satEnd = satReadCount;
                double closestTime = INFINITY;
                pthread_cond_wait(&g_Cond, &g_Mutex);
                for (int j = 0; j < satEnd; j++) {
                
                    //Check if coordinates matches
                    if (satReadings[j].sat_coord1 == sensorAlert.sensor_coord1 && satReadings[j].sat_coord2 == sensorAlert.sensor_coord2) {

                        //check if time matches between time threshold
                        double thisTime = difftime(sensorAlert.sensorTime, satReadings[j].satTime);
                        if (thisTime <= timeTolerance && thisTime < closestTime)
                            closestTime = thisTime;       
                            satReadMatch.sat_coord1 = satReadings[j].sat_coord1;
                            satReadMatch.sat_coord2 = satReadings[j].sat_coord2;
                            satReadMatch.satHeight = satReadings[j].satHeight;
                            satReadMatch.satTime = satReadings[j].satTime;
                    }
                }
                pthread_mutex_unlock(&g_Mutex);       

                if (satReadMatch.sat_coord1 != -1) {
                    if ((abs(satReadMatch.satHeight-sensorAlert.sensorHeight)) <= heightTolerance) {
                        alertType = 1;    //If this is a true alert since matches the coordinates and satellite value water level
                        sensorTrueAlerts[sensorAlert.sensorRank] +=1;
                    } else    //If this is a false alert since matches the coordinates but not satellite value water level
                        sensorFalseAlerts[sensorAlert.sensorRank] +=1;
                } else {
                    // no matching sat rec - set vals - still log record
                    satReadMatch.sat_coord1 = -1;
                    satReadMatch.sat_coord2 = -1;
                    satReadMatch.satHeight = 0.0;
                    satReadMatch.satTime = time(NULL);
                    sensorFalseAlerts[sensorAlert.sensorRank] +=1;
                }
                
                //Write the report into file
                writeToLog(baseIterCount, sensorAlert.sensorRank, sensorAlert.sensor_coord1, sensorAlert.sensor_coord2, satReadMatch.sat_coord1, satReadMatch.sat_coord2, sensorAlert.sensorTime, satReadMatch.satTime, sensorAlert.sensorHeight, satReadMatch.satHeight, sensorAlert.similarCount, sensorAlert.neighRank, sensorAlert.neigh_coord1, sensorAlert.neigh_coord2, sensorAlert.neighHeight, alertType, sensorAlert.numMsgs);
            }
           
        }

        
        
        //free the calloc memory
        free(alertSensorFlags);
        
        //Terminate if completed the specified number of iterations for the base
        baseIterCount++;
        if (baseIterCount >= nbaseIters)
            baseSentinelValue = 0;

    }
    
    //Finished the threads
    pthread_join(hThread[0], NULL);
    pthread_join(hThread[1], NULL);
    // Clean up
	pthread_cond_destroy(&g_Cond);
	pthread_mutex_destroy(&g_Mutex);

    // Tell to sensors to end
    int val_ = 0;
    for (int z=1; z<=(nrows*ncols); z++) {
        MPI_Isend(&val_, 1, MPI_INT, z, sendTerminationTag, commWorld, &request[z]);
    }



    
    // print summary/write summary to log after completion of base station
    file = fopen("logs.txt", "a");
    fprintf(file, "\n\n=====================    SUMMARY    =====================\n");
    /*
    Content: total iterations completed, reason for exit, total comm time, per sensor - matched alert count, false alert cocunt, sum t/f alerts
    */
    if (userSentinelValue == 0)
        fprintf(file, "User ended program. %d iterations completed.\n", baseIterCount);
    else if (baseSentinelValue == 0)
        fprintf(file, "Base completed all %d iterations of the program.\n", nbaseIters);

    fprintf(file, "\nReporting Node\t\tTrue alerts\t\tFalse alerts\n");
    int totalT = 0;
    int totalF = 0;
    for (int i=0; i<numSensors; i++) {
        fprintf(file,"%d\t\t\t\t\t%d\t\t\t\t%d\n", i, sensorTrueAlerts[i], sensorFalseAlerts[i]);
        totalT += sensorTrueAlerts[i];
        totalF += sensorFalseAlerts[i];
    }
    fprintf(file, "\nTotal True alerts: %d\n", totalT);
    fprintf(file, "Total False alerts: %d\n", totalF);
    fprintf(file, "Total Communication time (seconds): %.3f\n", totalCommTime);

    fprintf(file, "Number of messages passed through the network when an alert is detected (sensor with neighbours and base): 2*(number of neighbours)+1\n");
    fprintf(file, "Total number of messages through the network due to alerts: %d\n", totalMsgCount);

    fprintf(file, "=========================================================\n");
    fclose(file); 

}
