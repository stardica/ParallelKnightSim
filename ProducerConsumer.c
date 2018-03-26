#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string.h>

#include <knightsim.h>
#include <rdtsc.h>

#define LATENCY 1

/*
#define NUMPAIRS 128
#define LOOP 20
*/

#define WORK 100000

/*
#define NUMPAIRS 256
#define LOOP 4000000
*/

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

#define NUMPAIRS 4096
#define LOOP 10000


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

	producer_init();

	consumer_init();

	srand(NUMPAIRS);

	//assert(ctxdestroylist->count >= NUM_THREADS);

	/*starts simulation and won't return until simulation
	is complete or all contexts complete*/

	printf("DESIM: Simulating %d pair(s) and %d interactions\n", NUMPAIRS, LOOP);

	sim_start = rdtsc();

	simulate();

	sim_time += (rdtsc() - sim_start);

	/*printf("etime time %llu\n", etime_time);*/

	//clean up
	KnightSim_clean_up();

	printf("End simulation time %llu cycles %llu pairs %d iters %llu\n",
			sim_time, CYCLE, NUMPAIRS, iters);

	return 1;
}

void producer_init(void){

	int i = 0;
	char buff[100];

	//create the user defined eventcounts
	ec_p = (eventcount**) calloc(NUMPAIRS, sizeof(eventcount*));

	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "ec_p_%d", i);
		ec_p[i] = eventcount_create(strdup(buff));
	}

	//create the user defined contexts
	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "producer_%d", i);
		context_create(producer, DEFAULT_STACK_SIZE, strdup(buff), i);
	}

	return;
}

void consumer_init(void){

	int i = 0;
	char buff[100];

	ec_c = (eventcount**) calloc(NUMPAIRS, sizeof(eventcount*));
	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "ec_c_%d", i);
		ec_c[i] = eventcount_create(strdup(buff));
	}

	for(i = 0; i < NUMPAIRS; i++)
	{
		memset(buff,'\0' , 100);
		snprintf(buff, 100, "consumer_%d", i);
		context_create(consumer, DEFAULT_STACK_SIZE, strdup(buff), i);
	}

	return;
}

void producer(context * my_ctx){
	//START sequential code!!!!!

	int my_pid = p_pid++;

	/*count_t i = 1;*/
	count_t j = 1;
	int m = 0;

	/*int iters = 0;*/

	/*unsigned int finished_id = 0;*/

	//END sequential code section!!!!!
	context_init_halt(my_ctx);

	/*fatal("in producer!\n");*/

	//printf("producer %d:\n\t init\n", my_pid);

	while(j <= LOOP)
	{
		//do work
		//printf("\t charging latency %d cycle %llu\n", LATENCY, CYCLE);
		//pause(rand() % LATENCY + 1, my_ctx);
		pause(1, my_ctx);

		//printf("producer_event %d cycle %llu\n", my_pid, CYCLE);

		//__sync_add_and_fetch(&iters, 1);


		for(m=0;m<WORK;m++)
		{
			iters++;
		}
		//printf("producer %d:\n", my_pid);
		//printf("\t advancing %s cycle %llu\n", ec_c[my_pid]->name, CYCLE);

		/*temp = rand() % LATENCY + 1;
		temp++;*/

		//printf("p after sync my pid %d\n", my_pid);

		/*if(!my_ctx)
			printf("ok found it\n");*/

		advance(ec_c[my_pid], my_ctx);

		//printf("after adv my pid %d\n", my_pid);

		//printf("\t await %s cycle %llu\n", ec_p[my_pid]->name, CYCLE);
		await(ec_p[my_pid], j, my_ctx);
		j++;
		//printf("producer %d:\n", my_pid);
		//printf("\t advanced and doing work cycle %llu\n", CYCLE);
	}

	/*printf("here\n");*/

	/*finished_id =  __sync_add_and_fetch(&finished, 1);
	if(finished_id >= finished)
		KnightSim_finished = true;*/

	return;
}

void consumer(context * my_ctx){
	//START sequential code!!!!!
	int my_pid = c_pid++;
	count_t i = 1;

	/*int iters = 0;*/

	int m = 0;

	/*int temp = 0;*/

	//END sequential code section!!!!!
	context_init_halt(my_ctx);

	/*fatal("in consumer!\n");*/
	//printf("consumer %d:\n\t init\n", my_pid);
	/*if(my_ctx->id == 4)
			e_start = rdtsc();*/

	while(1)
	{
		//await work
		//printf("\t await %s\n", ec_c[my_pid]->name);

		await(ec_c[my_pid], i, my_ctx);
		i++;

		//charge latency
		//printf("consumer %d:\n", my_pid);
		//printf("\t charging latency %d cycle %llu\n", LATENCY, CYCLE);

		/*temp = rand() % LATENCY + 1;
		temp++;*/

		//do work
		pause(1, my_ctx);
		//pause(rand() % LATENCY + 1, my_ctx);
		//printf("consumer %d:\n\t resuming from latency cycle %llu\n",my_pid, CYCLE);
		//printf("consumer_event %d cycle %llu\n", my_pid, CYCLE);

		//__sync_add_and_fetch(&iters, 1);
		for(m=0;m<WORK;m++)
		{
			iters++;
		}

		/*advance producer ctx*/
		//printf("\t advancing %s cycle %llu\n", ec_p[my_pid]->name, CYCLE);

		advance(ec_p[my_pid], my_ctx);
	}

	/*fatal("consumer should never exit cycle %llu\n", CYCLE);*/

	return;
}
