#include "elevator.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h> // For rand()

struct elevatorThreads {
	int current_floor;
	int dest_floor;
	int occupancy;
	int direction;

	pthread_mutex_t lock;
	pthread_cond_t prepareToExit;
	pthread_cond_t prepareToClose;
	pthread_cond_t reached[FLOORS]; // Signal passengers of the floor to get on	
	
	int numOfPassengersPerFloor[FLOORS]; // Waiting in each floor
	//int passengerNumInElev;

} elevatorThreads[ELEVATORS];

//pthread_mutex_t lock;
/*
int current_floor;
int direction;
int occupancy;
enum {ELEVATOR_ARRIVED=1, ELEVATOR_OPEN=2, ELEVATOR_CLOSED=3} state;
*/

void scheduler_init() {	
	int i, j;
	for(i=0; i<ELEVATORS; ++i){
		elevatorThreads[i].current_floor=0;
		elevatorThreads[i].dest_floor=0;
		elevatorThreads[i].direction=-1;
		elevatorThreads[i].occupancy=0;
		//state=ELEVATOR_ARRIVED;
		pthread_mutex_init(&elevatorThreads[i].lock, 0);

		pthread_cond_init( &elevatorThreads[i].prepareToExit, NULL);
		pthread_cond_init( &elevatorThreads[i].prepareToClose, NULL);
		for(j=0; j<FLOORS; ++j){
			pthread_cond_init( &elevatorThreads[i].reached[j], NULL);	
			elevatorThreads[i].numOfPassengersPerFloor[j]=0; // Waiting in each floor
		}

		//elevatorThreads[i].passengerNumInElev=0;
	}
}


void passenger_request(int passenger, int from_floor, int to_floor, 
											 void (*enter)(int, int), 
											 void(*exit)(int, int))
{	
	// wait for the elevator to arrive at our origin floor, then get in
	//int waiting = 1;

	int trackElevator = rand() % ELEVATORS; // Choose a random elevator
	
		// Lock the current elevator
		pthread_mutex_lock(&elevatorThreads[trackElevator].lock);
		// Elevator contains a passenger
		elevatorThreads[trackElevator].numOfPassengersPerFloor[from_floor] += 1;
		// Waits until elevator gets to this floor (from_floor)
		pthread_cond_wait(&elevatorThreads[trackElevator].reached[from_floor],
										 &elevatorThreads[trackElevator].lock);
		enter(passenger, trackElevator);
		elevatorThreads[trackElevator].occupancy++;

		// Push button
		elevatorThreads[trackElevator].dest_floor = to_floor; // Destination floor

		// Prepare to close door
		pthread_cond_signal( &elevatorThreads[trackElevator].prepareToClose);

		// Prepare to exit (waiting)
		pthread_cond_wait( &elevatorThreads[trackElevator].prepareToExit, 
											 &elevatorThreads[trackElevator].lock);

		// Elevator contains no passengers, passenger exits elevator
		elevatorThreads[trackElevator].numOfPassengersPerFloor[from_floor] -= 1;
		exit(passenger, trackElevator);
		elevatorThreads[trackElevator].occupancy--;

		// Signal to close door
		pthread_cond_signal( &elevatorThreads[trackElevator].prepareToClose);

		// Unlock initial lock
		pthread_mutex_unlock(&elevatorThreads[trackElevator].lock);

}

void elevator_ready(int elevator, int at_floor, 
										void(*move_direction)(int, int), 
										void(*door_open)(int), void(*door_close)(int)) {
	//if(elevator!=0) return;
	
	pthread_mutex_lock(&elevatorThreads[elevator].lock);

/*
	if(state == ELEVATOR_ARRIVED) {
		door_open(elevator);
		state=ELEVATOR_OPEN;
	}
	else if(state == ELEVATOR_OPEN) {
		door_close(elevator);
		state=ELEVATOR_CLOSED;
	}
*/
	// If occupied and at destination floor
	if(	elevatorThreads[elevator].occupancy > 0 &&
			elevatorThreads[elevator].dest_floor == at_floor){
		
		door_open(elevator);
		pthread_cond_signal( &elevatorThreads[elevator].prepareToExit);

		pthread_cond_wait( &elevatorThreads[elevator].prepareToClose, 
											 &elevatorThreads[elevator].lock);	
		door_close(elevator);
	}
	// If unoccupied and there exist at least one passenger at dest floor
	else if(elevatorThreads[elevator].occupancy == 0 &&
					elevatorThreads[elevator].numOfPassengersPerFloor[at_floor] > 0){

		door_open(elevator);
		// Signal that floor is reached
		pthread_cond_signal( &elevatorThreads[elevator].reached[at_floor]);
		// Wait for passenger to exit, then close door
		pthread_cond_wait( &elevatorThreads[elevator].prepareToClose,
											 &elevatorThreads[elevator].lock);
		door_close(elevator);
	}	
	else {
		if(at_floor==0 || at_floor==FLOORS-1) 
			elevatorThreads[elevator].direction*=-1;
		move_direction(elevator, elevatorThreads[elevator].direction);
		elevatorThreads[elevator].current_floor = at_floor + elevatorThreads[elevator].direction;
	}
	pthread_mutex_unlock( &elevatorThreads[elevator].lock);
}

