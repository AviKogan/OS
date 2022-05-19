
#ifndef EX2_SCHEDULER_H
#define EX2_SCHEDULER_H


#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "thread.h"

using namespace std;

#define LIB_ERROR(text) std::cerr << "thread library error: " << (text) << std::endl;
#define SYS_ERROR(text) std::cerr << "system error: " << (text) << std::endl;

#define FAILURE (-1)

#define SUCCESS 0

#define MAIN_THREAD_ID 0

#define NO_AVAILABLE_TID (-1)

/** Used for the _update_sleeping method, indicate the remaining quantums without the previous. */
#define SCHED_TOTAL_RUNNING_QUANTUMS_INIT 1

#define PREV_QUANTUM_WAS_THE_LAST 1

#define SAVE_MASK 1

#define CURRENTLY_SET_UP 0

#define SIGLONGJMP_RETURN_VALUE 1

#define TIMER_SECONDS_INIT 0

/** macro to init virtual timer. assumes that Scheduler::_singleton_scheduler already initiated. */
#define INIT_TIMER  int usec = Scheduler::_singleton_scheduler->_quantum_usecs;\
                    struct itimerval timer = { {TIMER_SECONDS_INIT, usec}, {TIMER_SECONDS_INIT, usec}}; \
                    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))  { \
                        SYS_ERROR("Failed to define 'setitimer' with ITIMER_VIRTUAL");           \
                        SYS_ERROR("Failed to define 'setitimer' with ITIMER_VIRTUAL");           \
                        exit(EXIT_FAILURE);                                                      \
                    }

typedef void (*thread_entry_point)();


/**
 * Scheduler that implements the Round-Robin scheduling policy.
 * Implemented with Singleton design pattern for 2 reasons:
 * 1. There is no need for 2 schedulers for the process.
 * 2. easy access to the scheduler in the SIGVTALRM signal handler - via get_instance().
 */
class Scheduler{

    /** */
    int _max_thread_num;

    int _quantum_usecs;

    /** Count the total quantums the thread was in running mode. */
    int _total_running_quantums;

    unordered_map<int, Thread*> _tid_to_thread_map;

    deque<int> _ready;

    unordered_map<int, int> _sleeping_tid_to_remaining_quantums;

    unordered_set<int> _blocked_threads_tid;

    int _running_thread;

    static void _timer_handler(int sig);

    static unique_ptr<Scheduler> _singleton_scheduler;

    /**
     * Scheduler ctor: Assumes quantum_usecs is non negative number.
     * */
    Scheduler(int quantum_usecs, int stack_size, int max_thread_num);

    int _get_next_tid();

    void _remove_thread_from_scheduler(int tid);

    void _run_first_in_ready();

    void _remove_tid_from_ready(int tid);

    bool _tid_exists(int tid);

    void _update_sleeping();

public:

    /**
     * dctor, used to free all the scheduler resources, first free all running thread resourced, than its resources.
     */
    ~Scheduler();

    /**
     * Singletons should not be cloneable.
     */
    Scheduler(Scheduler &other) = delete;

    /**
     * Singletons should not be assignable.
     */
    void operator=(const Scheduler &) = delete;

    /**
     * This is the static method that controls the access to the singleton
     * instance. it returns the client existing object stored in the static field.
     * prints error if called before the static field initialized (with Scheduler::init).
     */
    static Scheduler *get_instance();

    /**
     * On the first run, it creates a singleton object and places it
     * into the static field. On subsequent runs,
     * @return
     */
    static void init(int quantum_usecs, int stack_size, int max_thread_num);

    int spawn(thread_entry_point pFunction);

    int terminate_tid(int tid);

    int block_tid(int tid);

    int get_total_quantums() const;

    int get_thread_quantums(int tid);

    int get_tid() const;

    int resume_tid(int tid);

    int sleep_running(int num_quantums);
};


#endif //EX2_SCHEDULER_H
