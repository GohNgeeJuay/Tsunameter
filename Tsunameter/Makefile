ALL: asgn2 

asgn2: asgn2.c HelperFunctions.c SensorSubroutine.c
	mpicc asgn2.c HelperFunctions.c SensorSubroutine.c BaseStationSubroutine.c -o asgn2output -lm 
	
run:
	mpirun -oversubscribe -np $(proc) asgn2output
    
clean:
	/bin/rm -f asgn2output *.o
	/bin/rm -f logs.txt

	


