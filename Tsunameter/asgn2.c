/*
    Assignment 2 tsunameter. 

*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include "HelperFunctions.h" //include the other dependencies
#include "SensorSubroutine.h"
#include "BaseStationSubroutine.h"

#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1
#define MIN_WATER_HEIGHT 5000.0    //the minimum water height that could be randomly generated
#define MAX_WATER_HEIGHT 6800.0    //the maximum water height that could be randomly generated
#define WATER_THRESHOLD 6000.0     // the water height which is classified as a possible event
#define HEIGHT_TOLERANCE 200.0     //to count as similar
#define TIME_TOLERANCE 10.0        //time to match the satellite and sensor readings

//Message tags
#define SEND_REQUEST_TAG 10        //tag used when sending/receiving the request for average
#define SEND_AVG_TAG 11            //tag used when sending/receiving the average value
#define SEND_ALERT_BASE_TAG 12     //tag used when sending/receiving the alert to base
#define SEND_TERMINATION_TAG 13    //tag when terminating sensors


int main(int argc, char *argv[]) {

   
    float waterThreshold = 6000.0; //the default value for the water threshold. Can be adjusted by user input
    int ndims=2, world_size, world_rank, reorder, my_cart_rank, ierr;
    int nrows, ncols, nbaseIters;
    int nbr_i_lo, nbr_i_hi; //for up and down the neighbours 
    int nbr_j_lo, nbr_j_hi; //left and right neighbours

    MPI_Comm comm2D;    //for the virtual topology
    int dims[ndims],coord[ndims];
    int wrap_around[ndims];
    
    //The group for world and for sensors
    MPI_Group group_world, group_sensors; 
    MPI_Comm comm_sensors;
    
    
    //For the thread in the sensor nodes
    int provided;
    
    /* start up initial MPI environment */
    MPI_Init(&argc, &argv);  
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    
  
    /************************************************************
    */
    /* Get user inputs for the dimensions. Will repeatedly ask to get correct inputs
    /************************************************************
    */ 
    
    int userInputSuccessM = 0;    //checking if user inputs a digit
    int userInputSuccessN = 0; 
    int userInputSuccessI = 0;
    int userInputSuccessT = 0; 
    int userDimError = 1;    //Keep track of any errors of user inputs
   
    //Root rank will get the user inputs. 
    if (world_rank == 0){
        while (userDimError == 1) {
            userDimError = 0;

            printf("Enter the number of iterations for the base station to run (as integer): \n");
            userInputSuccessI = scanf("%d", &nbaseIters);    

            printf("The number of processes allocated for this tasks is: %d. Ensure that the total number of sensor nodes (m*n) is same as %d.\n", world_size, world_size-1);
            
            printf("Enter the dimension m (as integer): \n");
            userInputSuccessM = scanf("%d", &nrows);    
            
            printf("Enter the dimension n (as integer): \n");
            userInputSuccessN = scanf("%d", &ncols);  
            
            //Get the threshold value for water height
            printf("Enter the water threshold value (between %.1f and %.1f, else, default %.1f will be used) to report (as float): \n", WATER_THRESHOLD, MAX_WATER_HEIGHT, WATER_THRESHOLD);
            userInputSuccessT = scanf("%f", &waterThreshold);  
            
            //If the user inputs a non-digit value.
            if (userInputSuccessM != 1 || userInputSuccessN != 1 || userInputSuccessI != 1 || userInputSuccessT != 1){
                printf("ERROR: One of the user inputs is not correct.");
                userDimError = 1;
            }

            if (waterThreshold<6000 | waterThreshold>6500) {
                printf("WARNING: Water threshold outside recommended range. Default value %.1f used.\n", WATER_THRESHOLD);
                waterThreshold = WATER_THRESHOLD;
            }
            
            //Number of processes = number of rows * number of cols + 1 for the base station. 
            //Check if user input is not equals the number of processes specified in mpirun
            if( (nrows*ncols) != world_size -1) {
                printf("ERROR: (nrows*ncols)= %d*%d = %d != %d\n", nrows, ncols, nrows*ncols,world_size-1);
                userDimError = 1;
            } 
                        
            dims[0] = nrows; /* number of rows */
            dims[1] = ncols; /* number of columns */
        }
    }
    
    
    //Broadcast the variables to all other processes.
    MPI_Bcast( &nrows, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast( &ncols, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast( &waterThreshold, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
    MPI_Bcast( dims, 2, MPI_INT, 0, MPI_COMM_WORLD);
    
    
    
    /************************************************************
    */
    /* create grouping and comm for sensor nodes */
    /************************************************************
    */
    
    //Reference from: Mary Thomas San Diego Slides    
    //Reference from: https://mpitutorial.com/tutorials/introduction-to-groups-and-communicators/
    //Create group for sensors 
    int groupRanksToBeIncl[nrows*ncols]; //Have nrows*ncols number of sensor nodes
   
   
    //Start from rank 1 (ignore root rank 0 which is used for base station). 
    //Put the ranks inside the array to be included in the new group. 
    for (int proc = 1; proc <= nrows*ncols; proc++) {
        groupRanksToBeIncl[proc-1] = proc;
    }
    
    
    /* Get the group underlying MPI_COMM_WORLD */
    MPI_Comm_group(MPI_COMM_WORLD, &group_world);
    
    //The new group is "group_sensors" with ranks from user input
    MPI_Group_incl(group_world, nrows*ncols, groupRanksToBeIncl, &group_sensors);
    
    //Create new communicator for sensors called "comm_sensors" with ranks used as sensors
    MPI_Comm_create_group(MPI_COMM_WORLD, group_sensors,0, &comm_sensors);
    
    int comm_sensors_rank;     
    int comm_sensors_size; 

    if (comm_sensors == MPI_COMM_NULL) { //if this rank is not included as sensor node
        
        //if this is main process
        if (world_rank == 0){
        
            //Perform the base station subroutine
            baseStationSubroutine(waterThreshold, MAX_WATER_HEIGHT, nbaseIters, nrows, ncols, MPI_COMM_WORLD, SEND_ALERT_BASE_TAG, SEND_TERMINATION_TAG, nrows*ncols, HEIGHT_TOLERANCE, TIME_TOLERANCE ); 
        }
        
    } 
    
    else {    //if this rank is included as sensor node
    
        MPI_Comm_rank(comm_sensors, &comm_sensors_rank);  /* get my rank in this group */
        MPI_Comm_size(comm_sensors, &comm_sensors_size);
        
        
        /************************************************************
        */
        /* create cartesian topology for the sensor nodes */
        /************************************************************
        */
    
        //Create dimensions based on user inputs
        MPI_Dims_create(nrows*ncols, ndims, dims);
    
    
        /* create cartesian mapping */
        wrap_around[0] = wrap_around[1] = 0; //no wrap around for cartesian grid
        reorder = 1;    
        ierr =0;      //for error message
        ierr = MPI_Cart_create(comm_sensors, ndims, dims, wrap_around, reorder, &comm2D); //create virtual topology
        if(ierr != 0) printf("ERROR[%d] creating CART\n",ierr);
        
        
        /* find my coordinates in the cartesian communicator group */
        MPI_Cart_coords(comm2D, comm_sensors_rank, ndims, coord); //pass rank to get coordinates
        /* get my neighbors; axis is coordinate dimension of shift */
        MPI_Cart_shift( comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi);    //Get the top and bottom neighbours
        MPI_Cart_shift( comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi);    //Get the left and right neighbours
        //printf("Global rank: %d. Cart rank: %d. Coord: (%d, %d). Left: %d. Right: %d. Top: %d. Bottom: %d\n", world_rank, comm_sensors_rank, coord[0], coord[1], nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi);
    
        
        int neighbours[4] = {nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi};
        
         //Perform the sensor subroutine. Only the sensors will do this
        sensorRoutine(comm_sensors_rank, coord, neighbours, MIN_WATER_HEIGHT, MAX_WATER_HEIGHT, waterThreshold, comm2D, MPI_COMM_WORLD, SEND_REQUEST_TAG, SEND_AVG_TAG, SEND_ALERT_BASE_TAG, SEND_TERMINATION_TAG, HEIGHT_TOLERANCE);    
        
    }

    //Clean up
    if (world_rank != 0){
        printf("sensor rank %d terminated\n", world_rank-1);  
        MPI_Group_free( &group_sensors );
        MPI_Comm_free( &comm_sensors );
    }
    MPI_Group_free( &group_world );
    
    MPI_Finalize();
    return 0;
}
