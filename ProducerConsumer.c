#include <knightsim.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <assert.h>

#include <rdtsc.h>

#define LATENCY 1


/*
#define NUMPAIRS 128
#define LOOP 20
*/

#define NUMPAIRS 1024
#define LOOP 100 //add one extra cycle

/*
#define NUMPAIRS 512
#define LOOP 2000000
*/

/*
#define NUMPAIRS 1024
#define LOOP 1000000
*/

/*
#define NUMPAIRS 2048
#define LOOP 500000
*/

/*
#define NUMPAIRS 4096
#define LOOP 10000
*/


eventcount **ec_p;
eventcount **ec_c;

void producer(context * my_ctx);
void consumer(context * my_ctx);
void producer_init(void);
void consumer_init(void);

long long p_pid = 0;
long long c_pid = 0;

unsigned long long sim_start = 0;
unsigned long long sim_time = 0;

unsigned long long e_start = 0;


unsigned long long iters = 0;


int main(void){

	//user must initialize DESim
	KnightSim_init();

	//warning("need to set the thread id for each context\n");

	producer_init();

	consumer_init();

	/*starts simulation and won't return until simulation
	is complete or all contexts complete*/
	printf("Simulating %d pair(s) and %d interactions\n", NUMPAIRS, LOOP);

	sim_start = rdtsc();

	simulate();

	sim_time += (rdtsc() - sim_start);

	//clean up
	KnightSim_clean_up();

	printf("End simulation time %llu cycles %llu pairs %d iters %llu\n",
			sim_time, CYCLE, NUMPAIRS, iters);

	return 1;
}

void producer_init(void){

	int i = 0;
	int j = 0;
	char buff[100];

	int blk_size = NUMPAIRS/KNIGHTSIM_THREAD_COUNT;

	//create the user defined eventcounts
	ec_p = (eventcount**) calloc(NUMPAIRS, sizeof(eventcount*));


	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "ec_p_%d", i);
		ec_p[i] = eventcount_create(strdup(buff), thread_safe);
	}


	//create the user defined contexts
	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "producer_%d", i);

		context_create(producer, DEFAULT_STACK_SIZE, strdup(buff), j, thread_safe); //must provide a thread ID

		if(!(i % blk_size) && i >= blk_size)
		{
			j++;
		}

		assert(j >= 0 && j < KNIGHTSIM_THREAD_COUNT);
	}

	return;
}

void consumer_init(void){

	int i = 0;
	int j = 0;
	char buff[100];

	int blk_size = NUMPAIRS/KNIGHTSIM_THREAD_COUNT;

	ec_c = (eventcount**) calloc(NUMPAIRS, sizeof(eventcount*));
	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "ec_c_%d", i);
		ec_c[i] = eventcount_create(strdup(buff), thread_safe);
	}


	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "consumer_%d", i);
		context_create(consumer, DEFAULT_STACK_SIZE, strdup(buff), j, thread_safe);

		if(!(i % blk_size) && i >= blk_size)
		{
			j++;
		}

		assert(j >= 0 && j < KNIGHTSIM_THREAD_COUNT);
	}

	return;
}

void producer(context * my_ctx){
	//START sequential code!!!!!
	int my_pid = p_pid++;
	count_t j = 1;
	//END sequential code section!!!!!
	context_init_halt(my_ctx);

	while(j <= LOOP)
	{
		__sync_add_and_fetch(&iters, 1);

		//do work
		pause(1, my_ctx);

		advance(ec_c[my_pid], my_ctx);

		await(ec_p[my_pid], j++, my_ctx);
	}

	return;
}

void consumer(context * my_ctx){
	//START sequential code!!!!!
	int my_pid = c_pid++;
	count_t i = 1;
	//END sequential code section!!!!!
	context_init_halt(my_ctx);

	while(1)
	{
		await(ec_c[my_pid], i++, my_ctx);

		//charge latency
		pause(1, my_ctx);
		//do work

		//__sync_add_and_fetch(&iters, 1);

		advance(ec_p[my_pid], my_ctx);
	}

	return;
}
