#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "HelperFunctions.h"

//Reference to generate random floats: https://stackoverflow.com/a/44105089
float randomFloatWaterLevel(float min, float max){
    
    //Used Pid to ensure more randomness across different process: https://stackoverflow.com/a/8623196
    //Need to sleep for 1 second to give ensure unique time (if called in simulataneously by multiple ranks or too quickly)
    sleep(1); 
    srand(time(NULL)^ (getpid()<<16));
    
    float scale = rand() / (float) RAND_MAX; 
    float randomFloat =  min + scale * ( max - min );   
    
    //Round to 3 decimal places.
    //Reference: https://stackoverflow.com/a/1344261   
    return roundf(randomFloat * 1000) /1000;
}
