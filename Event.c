#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string.h>

#include <knightsim.h>
#include <rdtsc.h>

#define EVENTS 512
#define TOTAL_CYCLES 1000001

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

	printf("KnightSim Event: Simulating %d events @ %d total\n", EVENTS, TOTAL_CYCLES - 1);

	sim_start = rdtsc();

	simulate();

	sim_time += (rdtsc() - sim_start);

	//clean up
	KnightSim_clean_up();

	printf("End simulation time %llu cycles %llu pairs %d iters %d\n", sim_time, CYCLE - 2, EVENTS, iters);

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
		context_create(event, STACKSIZE, strdup(buff), i);
	}

	return;
}


void event(context * my_ctx){

	volatile int i = 0;

	context_init_halt(my_ctx);

	while(CYCLE < TOTAL_CYCLES)
	{
		//printf("event %d cycle %llu\n", my_ctx->id, CYCLE);
		//getchar();
		//__sync_add_and_fetch(&iters, 1);
		//iters++;
		//sim_start = rdtsc();
		while(i < 375)
		     i++;

		//printf("after %llu\n", rdtsc() - sim_start);
		//getchar();

		pause(1, my_ctx);
		i = 0;
	}

	//context will terminate
	return;
}
