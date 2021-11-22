#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "SensorSubroutine.h"
#include "HelperFunctions.h"
#define NUM_THREADS 1
#define SEND_TERMINATION_TAG 13
#define NEIGHBOUR_TERMINATED_FLAG 14

//Struct for base station alert
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

int endFlag = 0;
// base end message check
void *baseEnd(void *pArg) {
    MPI_Comm commWorld = pArg;

    while (endFlag == 0) { //Check repeatedly for the termination signal from the base station. Store in the endFlag variable
        sleep(0.1);
        MPI_Iprobe(0, SEND_TERMINATION_TAG, commWorld, &endFlag, MPI_STATUS_IGNORE);
    }
    int val_;    //not really needed to receive the message, just for completion sake
    MPI_Recv( &val_, 1, MPI_INT,0 , SEND_TERMINATION_TAG, commWorld, MPI_STATUS_IGNORE);
    return NULL;
}

void sensorRoutine(int rank, int coord[2], int neighbours[4],float minWaterHeight, float maxWaterHeight, float waterThreshold, MPI_Comm comm2D, MPI_Comm commWorld, int sendRequestTag, int sendAvgTag, int sendAlertBaseTag, int sendTerminationTag, float heightTolerance){
    
    
    //IMPORTANT: One cycle (window for moving average) = 20 seconds, each entry of number is every 5 seconds. Can modify if needed
     
    //Local variables    
    float randomNumbers[4];    //for the randomly generated numbers. There will be 4 numbers in total
    float movingAverage = 0.0; //float value for the average random number
    float randomFloat;         //random float value for water level
    int alertFlag = 0;         //flag if the rank has an alert to report  
    int position = 0;          //for the packing of buffer
    int iter = 1;              //Keep track of number of iteration of subroutine
    int countRequest = 0;      //the count of requests that need to be tracked. 
    
    //1 index to handle the receiving of the message from base station, 4 for sending request message to 4 potential neighbours
    //4 for receiving average from the 4 potential neighbours, and 4 for sending the average to 4 neighbours if needed 
    MPI_Request request[13];  
    
    //Generate the first 4 random numbers before getting the moving average value
    for (int i = 0; i < 4; i++){
        randomNumbers[i] = randomFloatWaterLevel(minWaterHeight, maxWaterHeight);
    }
    
    //Find initial moving average value. Sum first
    for (int i = 0; i < 4; i++){
        movingAverage += randomNumbers[i]; 
    }
    
    //Divide to get average
    movingAverage = movingAverage/4;
    
    //round the value to 3 dp
    movingAverage = roundf(movingAverage * 1000) /1000;
    
    //This will keep track of the oldest random float in the randomNumbers array. New floats will overwrite the index
    int oldestFloat = 0;  


    pthread_t hThread[NUM_THREADS];
    int threadNum[NUM_THREADS];  
    threadNum[0] = 0;
	pthread_create(&hThread[0], NULL, baseEnd, commWorld);

    //Continue while base station has not signal it to stop    
    while (endFlag == 0){
        
    
        //Reset the counter which manages all the request once all of the requests are completed
        countRequest = 0;
        
        alertFlag = 0; //reset the flag for new round
        
        //The number of messages to communicate with base station to send alert
        int numMessages = 0; 
        
        //Wait for 4 seconds for the next interval. The generation of random float level will take 1 second. In total 5 seconds.
        sleep(4);
        
        //A new random float will replace the oldest float in the array
        randomFloat = randomFloatWaterLevel( minWaterHeight, maxWaterHeight);
        
        
        //Get the new movingAverage value
        // ((old sum) - oldest float + new float) /4 = new average 
        movingAverage = ((movingAverage * 4) - randomNumbers[oldestFloat] + randomFloat) / 4;
        
        //Round to 3dp
        movingAverage = roundf(movingAverage * 1000) /1000;
        
        randomNumbers[oldestFloat] = randomFloat; //replace the oldest float
        
        //increase the index of oldest float
        oldestFloat = (oldestFloat + 1) % 4;
            
        //receive values from all neighbours. Default value for neighbours is 0.0
        float receivedValues[4] = {0.0,0.0,0.0,0.0};
  
        //Array to keep track if the neighbours are having alert (they will send a message to this process to ask for movingAverage
        int alertNeighbourFlags[4] = {0,0,0,0};
        
        struct alert thisMsg;
        int countSimilar = 0;
                        

        
        //If the moving average is greater than the predefined threshold, check with neighbour values
        if (movingAverage > waterThreshold && endFlag == 0){  
            alertFlag = 1;
            
            for (int i = 0; i < 4; i++){ //send a message to get the average from the neigbours
                if (neighbours[i] != -2){ //if this rank have this active neighbour 
                    
                    //Send a request to get the average from neighbour i. 
                    MPI_Isend(&alertFlag, 1, MPI_INT, neighbours[i], sendRequestTag, comm2D,  &request[countRequest]);  
                    //Receive the average from neighbour i
                    MPI_Irecv(&receivedValues[i],1,MPI_FLOAT, neighbours[i], sendAvgTag, comm2D, &request[countRequest+1] );
                    
                    countRequest += 2; //increase the count
                    numMessages += 2;
                    
                }
            }
        } 
        
        for (int i = 0; i < 4; i++){
            if (neighbours[i] != -2){ //if this rank have this active neighbour
                
                //Check if this neighbour needs the movingAverage and store this flag inside the array
                MPI_Iprobe(neighbours[i],sendRequestTag,comm2D,&alertNeighbourFlags[i],MPI_STATUS_IGNORE);
                if (alertNeighbourFlags[i]!= 0){ //if the neighbour asked for the average, send it to them
                    MPI_Isend(&movingAverage, 1, MPI_FLOAT, neighbours[i], sendAvgTag, comm2D, &request[countRequest]);
                    countRequest += 1;
                }
            }
        }
        
        
        int receivedAllValues = 1;
        for (int i = 0; i < 4; i++){
            if (alertFlag == 1 && neighbours[i] != -2 && receivedValues[i] == 0.0)
                receivedAllValues = 0; //if did not received the average from this neighbour yet
        }
        
        int flagsRequestDone[4] = {0,0,0,0};
        //This block of code will repeats until the alerted node gets the average from all of its neighbours
        while (receivedAllValues == 0){ //while the rank is still waiting for an average value
            
            receivedAllValues = 1;
            //check if received all average values from existing neighbours
            for (int i = 0; i < 4; i++){
                if (alertFlag == 1 && neighbours[i] != -2 && receivedValues[i] == 0){
                    receivedAllValues = 0;
                }
            }            
            //if there is an signal to stop, dont wait for it anymore
            if (endFlag == 1)
                receivedAllValues = 1; 
        }
        
        
        //In case you want to test the values, uncomment this section
        //printf("Iteration %d. Rank %d. Moving average = %.3f\n", iter, rank, movingAverage);
        //for (int i = 0; i< 4; i++)
        //    if (neighbours[i] != -2)
        //        printf("Iteration %d. Rank %d. Received value from %d = %.3f\n", iter, rank, neighbours[i], receivedValues[i]);


        if (endFlag == 0){ //only continue with checking and sending alert to base station if not terminating
            for (int i = 0; i<4; i++){
            
                //Check if similar to the movingAverage +- threshold
                if (receivedValues[i] >= movingAverage - heightTolerance && receivedValues[i] <= movingAverage + heightTolerance)
                    countSimilar += 1;
            }
            
            //Check to see if need to report to base station
            if (countSimilar >= 2){
                
                //added new message to base station
                numMessages += 1;
                thisMsg.numMsgs = numMessages;
                
                thisMsg.sensorRank = rank;
                thisMsg.sensor_coord1 = coord[0];
                thisMsg.sensor_coord2 = coord[1];
                thisMsg.sensorHeight = movingAverage;
                thisMsg.similarCount = countSimilar;
                time(&thisMsg.sensorTime);
                for(int i = 0; i<4; i++) {
                    if (neighbours[i] != -2){
                        int thisCoord[2];
                        MPI_Cart_coords(comm2D, neighbours[i], 2, thisCoord);
                        thisMsg.neigh_coord1[i] = thisCoord[0];
                        thisMsg.neigh_coord2[i] = thisCoord[1];
                        thisMsg.neighHeight[i] = receivedValues[i];
                        thisMsg.neighRank[i] = neighbours[i];
                    }
                    else{ //give a default value to the non existant neighbour rank. Used for checking later
                        thisMsg.neighRank[i] = -2;
                    }
                }
                // send alert
                MPI_Send(&thisMsg, sizeof(struct alert), MPI_CHAR, 0, sendAlertBaseTag, commWorld);               
               
            }
            iter += 1;

        }    

    }
    pthread_join(hThread[0], NULL); //join back the thread after completion
}
