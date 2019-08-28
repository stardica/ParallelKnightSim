#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <assert.h>
#include <knightsim.h>

#include <rdtsc.h>

#define EVENTS 1024
#define TOTAL_CYCLES 101 //add one extra cycle

#define STACKSIZE 16384

void event(context * my_ctx);
void event_init(void);

long long p_pid = 0;

unsigned long long sim_start = 0;
unsigned long long sim_time = 0;

volatile int iters = 0;


int main(void){

	//user must initialize DESim
	KnightSim_init();

	event_init();

	/*starts simulation and won't return until simulation
	is complete or all contexts complete*/

	printf("Simulating %d events @ %d total\n", EVENTS, TOTAL_CYCLES - 1);

	sim_start = rdtsc();

	simulate();

	sim_time += (rdtsc() - sim_start);

	//clean up
	KnightSim_clean_up();

	printf("End simulation time %llu cycles %llu events %d iters %d\n", sim_time, CYCLE - 2, EVENTS, iters);

	return 1;
}

void event_init(void){

	int i = 0;
	char buff[100];

	//create the user defined contexts
	for(i = 0; i < EVENTS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "events_%d", i);

		//printf("thread id %d\n", (i % KNIGHTSIM_THREAD_COUNT));
		assert((i % KNIGHTSIM_THREAD_COUNT) >= 0 && (i % KNIGHTSIM_THREAD_COUNT) < KNIGHTSIM_THREAD_COUNT);

		context_create(event, STACKSIZE, strdup(buff), (i % KNIGHTSIM_THREAD_COUNT), non_thread_safe);
	}

	return;
}


void event(context * my_ctx){
	//START sequential code!!!!!
	//volatile int i = 0;
	//END sequential code section!!!!!
	context_init_halt(my_ctx);

	while(CYCLE < TOTAL_CYCLES)
	{

		__sync_add_and_fetch(&iters, 1);

		pause(1, my_ctx);
	}

	//context will terminate
	return;
}
