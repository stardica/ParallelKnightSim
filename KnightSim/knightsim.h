#ifndef __KnightSim_H__
#define __KnightSim_H__

#include <pthread.h>

extern pthread_barrier_t etime_barrier;
extern pthread_barrier_t start_barrier;

#ifndef DEFAULT_STACK_SIZE
#define DEFAULT_STACK_SIZE 16384
#endif

#ifndef THREADSIZE
#define THREADSIZE (NUM_THREADS + 10)
#endif

//you can set num threads here or do it in the make file.
#ifndef NUM_THREADS
#define NUM_THREADS 1
#endif

#ifndef HASHSIZE
#define HASHSIZE 16
#endif


#define STK_OVFL_MAGIC 0x12349876 /* stack overflow check */

#ifdef TIME_64
typedef unsigned long long Time_t;
#else
typedef long long Time_t;
#endif

# if __WORDSIZE == 64
typedef long int __jmp_buf[8];
# elif defined  __x86_64__
__extension__ typedef long long int __jmp_buf[8];
# else
typedef int __jmp_buf[6];
# endif

typedef Time_t count_t;
typedef __jmp_buf jmp_buf;

/*our assembly fucntion (.s) files these store
and load CPU register values. For KnightSim
the stack pointer and instruction pointer are
all we really care about.*/
#if defined(__linux__) && defined(__x86_64)

int setjmp64_2(jmp_buf __env);
void longjmp64_2(jmp_buf __env, int val);
long long encode64(long long val);
long long decode64(long long val);
long long get_stack_ptr64(void);

#elif defined(__linux__) && defined(__i386__)

int setjmp32_2(jmp_buf __env);
void longjmp32_2(jmp_buf __env, int val);
int encode32(int val);
int decode32(int val);

#else
#error Unsupported machine/OS combination
#endif

struct list_t{
	char *name;
	int count;  /* Number of elements in the list */
	int size;  /* Size of allocated vector */
	int head;  /* Head element in vector */
	int tail;  /* Tail element in vector */
	void **elem;  /* Vector of elements */
};

//Eventcount objects
struct eventcount_t{
	char * name;		/* string name of event count */
	long long id;
	struct list_t *ctxlist;
	count_t count;		/* current value of event */
	struct context_t *ctx_list;
};

//Context objects
struct context_t{
	jmp_buf buf;		/*state */
	char *name;			/* task name */
	int id;				/* task id */
	int unique_id;
	count_t count;		/* argument to await */
	void (*start)(struct context_t *);	/*entry point*/
	unsigned magic;		/* stack overflow check */
	char *stack;		/*stack */
	int stacksize;		/*stack size*/
	struct context_t * batch_next;
};

//Context objects
struct thread_t{
	jmp_buf home_buf;		/*state */
	pthread_t thread;
};

typedef struct context_t context;
typedef struct eventcount_t eventcount;
typedef struct list_t list;
typedef struct thread_t thread;

struct context_data_t{

	context * ctx_ptr;
	int context_id;
};

typedef int bool;
enum {false, true};

extern bool KnightSim_finished;

typedef struct context_data_t context_data;

#define MAGIC_STACK_NUMBER 4

extern jmp_buf main_context;
extern jmp_buf halt_context;

extern context *current_context;

extern list *ecdestroylist;

extern eventcount *etime;

#define CYCLE etime->count

extern unsigned long long etime_start;
extern unsigned long long etime_time;

//KnightSim user level functions
void KnightSim_init(void);
eventcount *eventcount_create(char *name);
void context_create(void (*func)(context *), unsigned stacksize, char *name, int id);
void simulate(void);
void await(eventcount *ec, count_t value, context *my_ctx);
void advance(eventcount *ec, context *my_ctx);
void pause(count_t value, context * my_ctx);
void context_init_halt(context * my_ctx);
void thread_recover(void);

//KnightSim private functions
void ctx_hash_insert(context *context_ptr, unsigned int row, unsigned int col);
void eventcount_init(eventcount * ec, count_t count, char *ecname);
void eventcount_destroy(eventcount *ec);
void context_init(context *new_context);
void context_start(void);
void context_terminate(context * my_ctx);
int context_simulate(jmp_buf buf);
void context_end(jmp_buf buf);
void context_select(int id);
void context_switch(context *ctx_ptr);
void context_destroy(context *ctx_ptr);
void KnightSim_clean_up(void);

int set_jump(jmp_buf buf);
void long_jump(jmp_buf buf);
void thread_pool_create(void);
void *thread_start(void * threadid);
void thread_set_affinity(long core_id);

//KnightSim/PKnightSim util stuff
#define FFLUSH fflush(stderr); fflush(stdout);

void KnightSim_pause(void);
void KnightSim_dump_queues(void);

void warning(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void fatal(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

#define LIST_FOR_EACH_L(list_ptr, iter, iter_start_value) \
	for ((iter) = iter_start_value; (iter) < (list_ptr->count); (iter)++)

#define LIST_FOR_EACH_LG(list_ptr, iter, iter_start_value) \
	for ((iter) = iter_start_value; (iter) <= (KnightSim_list_count(list_ptr)); (iter)++)

#define INLIST(X) (((X) + list_ptr->size) % list_ptr->size)

list *KnightSim_list_create(unsigned int size);
void KnightSim_list_insert(list *list_ptr, int index, void *elem);
void *KnightSim_list_get(list *list_ptr, int index);
int KnightSim_list_count(list *list_ptr);
void KnightSim_list_enqueue(list *list_ptr, void *elem);
void *KnightSim_list_dequeue(list *list_ptr);
void KnightSim_list_add(list *list_ptr, void *elem);
void KnightSim_list_grow(list *list_ptr);
void *KnightSim_list_remove_at(list *list_ptr, int index);
void *KnightSim_list_remove(list *list_ptr, void *elem);
int KnightSim_list_index_of(list *list_ptr, void *elem);
void KnightSim_list_clear(list *list_ptr);
void KnightSim_list_free(list *list_ptr);

#endif /*__KnightSim_H__*/
