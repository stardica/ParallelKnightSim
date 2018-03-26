#define _GNU_SOURCE
#include "knightsim.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include "../include/rdtsc.h"

/* Globals*/
list *ctxdestroylist = NULL;
list *ctxlist = NULL;
list *ecdestroylist = NULL;
list *threadlist = NULL;

context *last_context = NULL;
context *current_context = NULL;
context *ctxhint = NULL;
context *curctx = NULL;
context *terminated_context = NULL;

eventcount *etime = NULL;

jmp_buf context_init_halt_buf;

long long ecid = 0;
long long ctxid = 0;
long long threadid = 0;

pthread_barrier_t etime_barrier;
pthread_barrier_t start_barrier;

/*pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t barrier_condition = PTHREAD_COND_INITIALIZER;*/

unsigned int unique_context_id = 0;
unsigned int num_columns =  NUM_THREADS - 1;

#define HASH_ROW_BY 0xF
#define HASH_COL_BY num_columns
context *ctx_hash_table[HASHSIZE][NUM_THREADS];
volatile int ctx_hash_table_count = 0;

//sim start
/*bool KnightSim_finished = false;*/

//table for thread counters
thread threads[THREADSIZE];
volatile int threads_created = 0;
volatile int threads_running = NUM_THREADS;

volatile bool global_barrier_sense = true;

unsigned long long ks_start = 0;
unsigned long long ks_end = 0;

void KnightSim_init(void){

	char buff[100];

	//hash table size should be a power of two
	if((HASHSIZE & (HASHSIZE - 1)) != 0)
		fatal("This is the optimized version of KnightSim.\n"
				"HASHSIZE = %d but must be a power of two.\n", HASHSIZE);

	//num threads should be a power of two
	if((NUM_THREADS & (NUM_THREADS - 1)) != 0)
		fatal("This is the optimized version of KnightSim.\n"
				"NUM_THREADS = %d but must be 1 or a power of two.\n", NUM_THREADS);

	//other globals
	ctxdestroylist = KnightSim_list_create(4);
	//ctxlist = KnightSim_list_create(4);
	ecdestroylist = KnightSim_list_create(4);

	//set up etime
	memset(buff,'\0' , 100);
	snprintf(buff, 100, "etime");
	etime = eventcount_create(strdup(buff));

	//create the thread pool
	printf("---KnightSim Parallel Mode With %d threads---\n", NUM_THREADS);
	thread_pool_create();

	return;
}

void thread_pool_create(void){

	//init barrier
	pthread_barrier_init(&etime_barrier, NULL, NUM_THREADS);
	pthread_barrier_init(&start_barrier, NULL, NUM_THREADS + 1);

	long i;
	//create next thread
	for(i = 0; i < NUM_THREADS; i++)
	{
		pthread_create(&threads[i].thread, NULL, thread_start, (void *)i);
		printf("---PDESim thread id %lu created---\n", i);
	}

	while(threads_created != NUM_THREADS){}; //spin till all of the threads are made

	return;
}


void thread_set_affinity(long core_id){

   cpu_set_t cpuset;
   pthread_t thread;

   thread = pthread_self();

   /* Set affinity mask to include CPUs 0 to 7 */
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

   return;
}


eventcount *eventcount_create(char *name){

	eventcount *ec_ptr = NULL;

	ec_ptr = (eventcount *)malloc(sizeof(eventcount));
	assert(ec_ptr);

	eventcount_init(ec_ptr, 0, name);

	//for destroying the ec later
	KnightSim_list_insert(ecdestroylist, 0, ec_ptr);

	return ec_ptr;
}

void ctx_hash_insert(context *context_ptr, unsigned int row, unsigned int col){

	/*assert(context_ptr);
	assert(context_ptr->batch_next == NULL);*/
	/*start = rdtsc();*/

	if(!ctx_hash_table[row][col])
	{
		//nothing here
		ctx_hash_table[row][col] = context_ptr;
		ctx_hash_table[row][col]->batch_next = NULL;
		__sync_add_and_fetch(&ctx_hash_table_count, 1); //serialization point
		/*end = rdtsc();
		printf("time was %llu\n", end - start);*/
		//printf("creating new record row %d col %d cycle %llu\n", row, col, etime->count);
	}
	else
	{
		//something here stick it at the head
		/*assert(context_ptr->count == ctx_hash_table[where]->count);*/
		context_ptr->batch_next = ctx_hash_table[row][col]; //set new ctx as head
		ctx_hash_table[row][col] = context_ptr; //move old ctx down
		//printf("adding to record row %d col %d cycle %llu\n", row, col, etime->count);
	}

	return;
}



void context_create(void (*func)(context *), unsigned stacksize, char *name, int id){

	/*stacksize should be multiple of unsigned size */
	assert ((stacksize % sizeof(unsigned)) == 0);
	assert(etime);

	context *new_context_ptr = NULL;

	new_context_ptr = (context *) malloc(sizeof(context));
	assert(new_context_ptr);

	new_context_ptr->count = etime->count + 1 ; //start at cycle 1
	new_context_ptr->name = name;
	new_context_ptr->id = id;
	new_context_ptr->unique_id = unique_context_id++;
	new_context_ptr->stack = (char *)malloc(stacksize);
	assert(new_context_ptr->stack);
	/*printf("ptr 0x%08llx\n", (long long)new_context_ptr->stack);*/
	new_context_ptr->stacksize = stacksize;
	new_context_ptr->magic = STK_OVFL_MAGIC; // for stack overflow check
	new_context_ptr->start = func; /*assigns the head of a function*/

	//move data unto the context's stack
	context_data * context_data_ptr = (context_data *) malloc(sizeof(context_data));
	context_data_ptr->ctx_ptr = new_context_ptr;
	context_data_ptr->context_id = new_context_ptr->id;//new_context_ptr->id;

	//copy data to context's stack then free the local copy.
	memcpy((void *)new_context_ptr->stack, context_data_ptr, sizeof(context_data));
	free(context_data_ptr);

	context_init(new_context_ptr);

	//start up new context
#if defined(__linux__) && defined(__i386__)
	if (!setjmp32_2(context_init_halt_buf))
	{
	  longjmp32_2(new_context_ptr->buf, 1);
	}
#elif defined(__linux__) && defined(__x86_64)
	if (!setjmp64_2(context_init_halt_buf))
	{
		longjmp64_2(new_context_ptr->buf, 1);
	}
#else
#error Unsupported machine/OS combination
#endif

	//changes
	 //make sure the init is above any context create funcs
	/*printf("thraeds %d\n", NUM_THREADS);*/

	/*printf("%s row %d col %d\n", new_context_ptr->name,
				(unsigned int) new_context_ptr->count & HASH_ROW_BY,
				(unsigned int) new_context_ptr->unique_id & HASH_COL_BY);*/

	ctx_hash_insert(new_context_ptr,
					new_context_ptr->count & HASH_ROW_BY,
					new_context_ptr->unique_id & HASH_COL_BY);

	//put in etime's ctx list
	//KnightSim_list_enqueue(etime->ctxlist, new_context_ptr);

	//for destroying the context later
	KnightSim_list_enqueue(ctxdestroylist, new_context_ptr);

	return;
}

void eventcount_init(eventcount * ec, count_t count, char *ecname){

	ec->name = ecname;
	ec->id = ecid++;
	ec->count = count;
	ec->ctx_list = NULL;
	ec->ctxlist = KnightSim_list_create(4);
	//pthread_mutex_init(&ec->count_mutex, NULL); //only used for parallel implementations
    return;
}

void advance(eventcount *ec, context *my_ctx){

	/* advance the ec's count */
	ec->count++;

	/*Check if there is a context and if it is ready to run*/
	if(ec->ctx_list && ec->ctx_list->count == ec->count)
	{
		//there is a context on this ec and it's ready
		ec->ctx_list->batch_next = my_ctx->batch_next;
		my_ctx->batch_next = ec->ctx_list;
		ec->ctx_list =  NULL;
	}

	return;
}


void context_terminate(context * my_ctx){

	/*we are deliberately allowing a context to terminate it self
	destroy the context and switch to the next context*/

	//set curr ctx to next ctx in list (NOTE MAYBE NULL!!)
	my_ctx = my_ctx->batch_next;

	if(my_ctx)
	{
		long_jump(my_ctx->buf);
	}
	else
	{
		//thread_recover();
		//long_jump(threads[pthread_self() % THREADSIZE].home_buf);
		long_jump(threads[pthread_self() % THREADSIZE].home_buf);
	}

	return;
}

void pause(count_t value, context * my_ctx){

	//we only ever pause on etime.
	//assert(my_ctx);

	value += etime->count; //thread safe read of etime.count

	//Get a pointer to the next context first NOTE MAYBE NULLL!!!!
	context *head_ptr = my_ctx;
	my_ctx = my_ctx->batch_next;

	//head_ptr->count = value;

	//insert the context into the hash table (its possible that contexts are reordering after being stolen).
	ctx_hash_insert(head_ptr,
			//(unsigned int) head_ptr->count & HASH_ROW_BY,
			value & HASH_ROW_BY,
			(unsigned int) head_ptr->unique_id & HASH_COL_BY);


	if(!set_jump(head_ptr->buf)) //update current context
	{
		if(my_ctx)
		{
			long_jump(my_ctx->buf);
		}
		else
		{
			//thread_recover();
			long_jump(threads[pthread_self() % THREADSIZE].home_buf);
		}
	}

	return;
}

void await(eventcount *ec, count_t value, context *my_ctx){

	/*todo
	check for stack overflows*/

	/*continue if the ctx's count is less
	 * than or equal to the ec's count*/
	if (ec->count >= value)
		return;

	/*the current context must now wait on this ec to be incremented*/
	if(!ec->ctx_list)
	{
		//set the count to await
		my_ctx->count = value;

		//have ec point to halting context
		ec->ctx_list = my_ctx;

		//set curr ctx to next ctx in list (NOTE MAYBE NULL!!)
		my_ctx = my_ctx->batch_next;

		//update the tail pointer in ec's ctx list
		//ec->ctx_list->batch_next = NULL;
	}
	else
	{
		//there is an ec waiting.
		fatal("await(): fixme handle more than one ctx in ec list name %s in ec %s id %d\n",
				current_context->name, ec->ctx_list->name, ec->ctx_list->id);
	}

	if(!set_jump(ec->ctx_list->buf)) //update current context
	{
		if(my_ctx) //if there is another context run it
		{
			long_jump(my_ctx->buf);
		}
		else //we are out of contexts so get the next batch
		{
			//thread_recover();
			long_jump(threads[pthread_self() % THREADSIZE].home_buf);
		}
	}

	return;
}



void simulate(void){
	//current_context = NULL;
	//last_context = NULL;

	/*unsigned int l = 0;
	unsigned int j = 0;

	for(l = 0; l < HASHSIZE; l ++)
		for(j = 0 ; j < NUM_THREADS; j++)
		{
			printf("i %d j %d\n", l, j);
			current_context = ctx_hash_table[l][j];

			while(current_context)
			{
				printf("found %s\n", current_context->name);
				current_context = current_context->batch_next;
			}

		}*/
	//fatal("my count %d\n", ctx_hash_table_count);

	/*for(i = 0; i < NUM_THREADS; i++)
		printf("col %d val %d\n", i, thread_hash_table_count[i]);*/

	//start simulation
	//pthread_barrier_wait(&start_barrier);

	//printf("setting barrier %d\n", global_barrier_sense);
	global_barrier_sense = false;

	/*ks_start = rdtsc();*/

	//wait for threads to finish
	int i = 0;
	//printf("waiting for threads\n");
	for(i = 0; i < NUM_THREADS; i++)
		pthread_join(threads[i].thread, NULL);

	//return to main
	return;
}



void *thread_start(void *threadid){
	//threads created we are ready to run.
	//int arrival_count;
	volatile bool local_barrier_sense = false;

	long id = (long)threadid;
	thread_set_affinity(id);
	__sync_add_and_fetch(&threads_created, 1);

	//barrier, wait for main to finish setting things up
	//pthread_barrier_wait(&start_barrier);
	//printf("gb %d lb %d\n", global_barrier_sense, local_barrier_sense);
	while (global_barrier_sense != local_barrier_sense){}; //true & false
														   //false & false

	//printf("id %d made it\n", (int)id);

	set_jump(threads[pthread_self() % THREADSIZE].home_buf);

	local_barrier_sense = !local_barrier_sense;            //false and true

	if(__sync_sub_and_fetch(&threads_running, 1)) //stall threads until last one arrives (last one is id 0)
	{
		//fatal("here gb %d lb %d\n", global_barrier_sense, local_barrier_sense);
		while(global_barrier_sense != local_barrier_sense){};
		//pthread_barrier_wait(&etime_barrier);
	}
	else
	{
		//printf("last to arive\n");
		etime->count++; //last guy (id 0) so increment etime.count
		threads_running = NUM_THREADS; //reset thread counter for next batch
		//printf("-----------cycle------------ %llu\n", etime->count);
		global_barrier_sense = local_barrier_sense;
		//pthread_barrier_wait(&etime_barrier);
	}

	context_select(id);

	return NULL;
}


/*void context_select(int id){

	get next ctx to run
	fatal("CS id %d\n", id);
	context *next_context = ctx_hash_table[etime->count & HASH_ROW_BY][id];

	if(next_context)
	{
		ctx_hash_table[etime->count & HASH_ROW_BY][id] = NULL;
		//__sync_sub_and_fetch(&ctx_hash_table_count, 1); //serialization point 3

		long_jump(next_context->buf); //off to context land
	}
	else
	{
		printf("really? id %d cycle %llu\n", id, CYCLE);
		//thread_recover();
		long_jump(threads[pthread_self() % THREADSIZE].home_buf);
	}

	ks_end = rdtsc();
	printf("sim end %llu\n", ks_end - ks_start);
	return;
}*/

void context_select(int id){

	/*get next ctx to run*/
	/*fatal("CS id %d\n", id);*/
	//printf("running id %d cycle %llu\n", id, etime->count);
	if(ctx_hash_table_count)
	{
		context *next_context = ctx_hash_table[etime->count & HASH_ROW_BY][id];

		if(next_context)
		{
			ctx_hash_table[etime->count & HASH_ROW_BY][id] = NULL;
			__sync_sub_and_fetch(&ctx_hash_table_count, 1); //serialization point 3

			long_jump(next_context->buf); //off to context land
		}
		else
		{
			//printf("really? id %d cycle %llu\n", id, CYCLE);
			//thread_recover();
			long_jump(threads[pthread_self() % THREADSIZE].home_buf);
		}
	}

	/*ks_end = rdtsc();
	printf("sim end %llu\n", ks_end - ks_start);*/
	return;
}

void context_start(void){

	//figure out who we are using the dark arts! XD
#if defined(__linux__) && defined(__x86_64)
	long long address = get_stack_ptr64() - (DEFAULT_STACK_SIZE - sizeof(context_data) - MAGIC_STACK_NUMBER);
	context_data * context_data_ptr = (context_data *)address;

	//jump
	(*context_data_ptr->ctx_ptr->start)(context_data_ptr->ctx_ptr);

	/*if current current ctx returns i.e. hits the bottom of its function
	it will return here. So, terminate the context and move on*/
	context_terminate(context_data_ptr->ctx_ptr);

#else
	fatal("context_start(): need to make get stack 32\n");
#endif

	fatal("context_start(): Should never be here!\n");
	return;
}

void context_init_halt(context * my_ctx){

//context has completed initial run jump back to init

	//set up new context
#if defined(__linux__) && defined(__i386__)
	if (!setjmp32_2(current_context->buf))
	{
	  longjmp32_2(context_init_halt_buf, 1);
	}
#elif defined(__linux__) && defined(__x86_64)
	if (!setjmp64_2(my_ctx->buf))
	{
		longjmp64_2(context_init_halt_buf, 1);
	}
#else
#error Unsupported machine/OS combination
#endif

}


int set_jump(jmp_buf buf){

#if defined(__linux__) && defined(__i386__)
	return setjmp32_2(buf);
#elif defined(__linux__) && defined(__x86_64)
	return setjmp64_2(buf);
#else
#error Unsupported machine/OS combination
#endif
}

void long_jump(jmp_buf buf){

#if defined(__linux__) && defined(__i386__)
	longjmp32_2(buf, 1);
#elif defined(__linux__) && defined(__x86_64)
	longjmp64_2(buf, 1);
#else
#error Unsupported machine/OS combination
#endif

	return;
}

int context_simulate(jmp_buf buf){

#if defined(__linux__) && defined(__i386__)
	//fatal("OMG AGAIN!!!!\n");
	return setjmp32_2(buf);
#elif defined(__linux__) && defined(__x86_64)
	//fatal("OMG\n");
	return setjmp64_2(buf);
#else
#error Unsupported machine/OS combination
#endif
}

void context_end(jmp_buf buf){

#if defined(__linux__) && defined(__i386__)
	longjmp32_2(buf, 1);
#elif defined(__linux__) && defined(__x86_64)
	longjmp64_2(buf, 1);
#else
#error Unsupported machine/OS combination
#endif

	return;
}

void context_init(context *new_context){

	/*these are in this function because they are architecture dependent.
	don't move these out of this function!!!!*/

	/*instruction pointer and then pointer to top of stack*/

#if defined(__linux__) && defined(__i386__)
	new_context->buf[5] = ((int)context_start);
	new_context->buf[4] = ((int)((char*)new_context->stack + new_context->stacksize - MAGIC_STACK_NUMBER));

#elif defined(__linux__) && defined(__x86_64)
	new_context->buf[7] = (long long)context_start;
	new_context->buf[6] = (long long)((char *)new_context->stack + new_context->stacksize - MAGIC_STACK_NUMBER);

#else
#error Unsupported machine/OS combination
#endif

	return;
}

void KnightSim_clean_up(void){

	int i = 0;
	eventcount *ec_ptr = NULL;
	context *ctx_ptr = NULL;

	//printf("KnightSim cleaning up\n");

	LIST_FOR_EACH_L(ecdestroylist, i, 0)
	{
		ec_ptr = (eventcount*)KnightSim_list_get(ecdestroylist, i);

		if(ec_ptr)
			eventcount_destroy(ec_ptr);
	}

	LIST_FOR_EACH_L(ctxdestroylist, i, 0)
	{
		ctx_ptr = (context*)KnightSim_list_get(ctxdestroylist, i);

		if(ctx_ptr)
			context_destroy(ctx_ptr);
	}

	KnightSim_list_clear(ctxdestroylist);
	KnightSim_list_free(ctxdestroylist);

	KnightSim_list_clear(ecdestroylist);
	KnightSim_list_free(ecdestroylist);

	return;
}

void context_destroy(context *ctx_ptr){

	//printf("CTX destroying name %s count %llu\n", ctx_ptr->name, ctx_ptr->count);

	assert(ctx_ptr != NULL);
	ctx_ptr->start = NULL;
	free(ctx_ptr->stack);
	free(ctx_ptr->name);
	free(ctx_ptr);
	ctx_ptr = NULL;

	return;
}


void eventcount_destroy(eventcount *ec_ptr){

	//printf("EC destroying name %s count %llu\n", ec_ptr->name, ec_ptr->count);

	free(ec_ptr->name);
	KnightSim_list_clear(ec_ptr->ctxlist);
	KnightSim_list_free(ec_ptr->ctxlist);
	free(ec_ptr);
	ec_ptr = NULL;

	return;
}

void KnightSim_dump_queues(void){

	int i = 0;
	int j = 0;
	eventcount *ec_ptr = NULL;
	context *ctx_ptr = NULL;

	printf("KnightSim_dump_queues():\n");

	printf("eventcounts\n");
	LIST_FOR_EACH_L(ecdestroylist, i, 0)
	{
		ec_ptr = (eventcount*)KnightSim_list_get(ecdestroylist, i);

		printf("ec name %s count %llu\n", ec_ptr->name, ec_ptr->count);
		LIST_FOR_EACH_L(ec_ptr->ctxlist, j, 0)
		{
			ctx_ptr = (context *)KnightSim_list_get(ec_ptr->ctxlist, j);
			printf("\tslot %d ctx %s ec count %llu ctx count %llu\n",
					j, ctx_ptr->name, ec_ptr->count, ctx_ptr->count);
		}

	}
	printf("\n");

	return;
}

//KnightSim Util Stuff
list *KnightSim_list_create(unsigned int size)
{
	assert(size > 0);

	list *new_list;

	/* Create vector of elements */
	new_list = (list*)calloc(1, sizeof(list));
	new_list->size = size < 4 ? 4 : size;
	new_list->elem = (void**)calloc(new_list->size, sizeof(void *));

	/* Return list */
	return new_list;
}


void KnightSim_list_free(list *list_ptr)
{
	free(list_ptr->name);
	free(list_ptr->elem);
	free(list_ptr);
	return;
}


int KnightSim_list_count(list *list_ptr)
{
	return list_ptr->count;
}

void KnightSim_list_grow(list *list_ptr)
{
	void **nelem;
	int nsize, i, index;

	/* Create new array */
	nsize = list_ptr->size * 2;
	nelem = (void**)calloc(nsize, sizeof(void *));

	/* Copy contents to new array */
	for (i = list_ptr->head, index = 0;
		index < list_ptr->count;
		i = (i + 1) % list_ptr->size, index++)
		nelem[index] = list_ptr->elem[i];

	/* Update fields */
	free(list_ptr->elem);
	list_ptr->elem = nelem;
	list_ptr->size = nsize;
	list_ptr->head = 0;
	list_ptr->tail = list_ptr->count;
}

void KnightSim_list_add(list *list_ptr, void *elem)
{
	/* Grow list if necessary */
	if (list_ptr->count == list_ptr->size)
		KnightSim_list_grow(list_ptr);

	/* Add element */
	list_ptr->elem[list_ptr->tail] = elem;
	list_ptr->tail = (list_ptr->tail + 1) % list_ptr->size;
	list_ptr->count++;

}


int KnightSim_list_index_of(list *list_ptr, void *elem)
{
	int pos;
	int i;

	/* Search element */
	for (i = 0, pos = list_ptr->head; i < list_ptr->count; i++, pos = (pos + 1) % list_ptr->size)
	{
		if (list_ptr->elem[pos] == elem)
			return i;
	}

	//not found
	return -1;
}


void *KnightSim_list_remove_at(list *list_ptr, int index)
{
	int shiftcount;
	int pos;
	int i;
	void *elem;

	/* check bounds */
	if (index < 0 || index >= list_ptr->count)
		return NULL;

	/* Get element before deleting it */
	elem = list_ptr->elem[(list_ptr->head + index) % list_ptr->size];

	/* Delete */
	if (index > list_ptr->count / 2)
	{
		shiftcount = list_ptr->count - index - 1;
		for (i = 0, pos = (list_ptr->head + index) % list_ptr->size; i < shiftcount; i++, pos = (pos + 1) % list_ptr->size)
			list_ptr->elem[pos] = list_ptr->elem[(pos + 1) % list_ptr->size];
		list_ptr->elem[pos] = NULL;
		list_ptr->tail = INLIST(list_ptr->tail - 1);
	}
	else
	{
		for (i = 0, pos = (list_ptr->head + index) % list_ptr->size; i < index; i++, pos = INLIST(pos - 1))
			list_ptr->elem[pos] = list_ptr->elem[INLIST(pos - 1)];
		list_ptr->elem[list_ptr->head] = NULL;
		list_ptr->head = (list_ptr->head + 1) % list_ptr->size;
	}

	list_ptr->count--;
	return elem;
}


void *KnightSim_list_remove(list *list_ptr, void *elem)
{
	int index;

	/* Get index of element */
	index = KnightSim_list_index_of(list_ptr, elem);

	/* Delete element at found position */
	return KnightSim_list_remove_at(list_ptr, index);
}


void KnightSim_list_clear(list *list_ptr)
{
	list_ptr->count = 0;
	list_ptr->head = 0;
	list_ptr->tail = 0;
	return;
}

void KnightSim_list_enqueue(list *list_ptr, void *elem)
{
	KnightSim_list_add(list_ptr, elem);
}


void *KnightSim_list_dequeue(list *list_ptr)
{
	if (!list_ptr->count)
		return NULL;

	return KnightSim_list_remove_at(list_ptr, 0);
}


void *KnightSim_list_get(list *list_ptr, int index)
{
	/*Check bounds*/
	if (index < 0 || index >= list_ptr->count)
		return NULL;

	/*Return element*/
	index = (index + list_ptr->head) % list_ptr->size;
	return list_ptr->elem[index];
}


void KnightSim_list_insert(list *list_ptr, int index, void *elem){

	int shiftcount;
	int pos;
	int i;

	/*Check bounds*/
	assert(index >= 0 && index <= list_ptr->count);

	/*Grow list if necessary*/
	if(list_ptr->count == list_ptr->size)
		KnightSim_list_grow(list_ptr);

	 /*Choose whether to shift elements on the right increasing 'tail', or
	 * shift elements on the left decreasing 'head'.*/
	if (index > list_ptr->count / 2)
	{
		shiftcount = list_ptr->count - index;
		for (i = 0, pos = list_ptr->tail;
			 i < shiftcount;
			 i++, pos = INLIST(pos - 1))
			list_ptr->elem[pos] = list_ptr->elem[INLIST(pos - 1)];
		list_ptr->tail = (list_ptr->tail + 1) % list_ptr->size;
	}
	else
	{
		for (i = 0, pos = list_ptr->head;
			 i < index;
			 i++, pos = (pos + 1) % list_ptr->size)
			list_ptr->elem[INLIST(pos - 1)] = list_ptr->elem[pos];
		list_ptr->head = INLIST(list_ptr->head - 1);
	}

	list_ptr->elem[(list_ptr->head + index) % list_ptr->size] = elem;
	list_ptr->count++;

	return;
}


void fatal(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "fatal: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	fflush(NULL);
	exit(1);
}

void warning(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
}
