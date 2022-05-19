

#include "scheduler.h"
#include "thread.h"
#include <vector>


/** The singleton scheduler object, used the unique_ptr for auto handling of de-allocation of the object when exiting */
unique_ptr<Scheduler> Scheduler::_singleton_scheduler(nullptr);

Scheduler::Scheduler(int quantum_usecs, int stack_size, int max_thread_num):
        _max_thread_num(max_thread_num), _quantum_usecs(quantum_usecs),
        _total_running_quantums(SCHED_TOTAL_RUNNING_QUANTUMS_INIT), _tid_to_thread_map{},
        _ready{}, _sleeping_tid_to_remaining_quantums{}, _blocked_threads_tid{},
        _running_thread(MAIN_THREAD_ID){

    Thread::stack_size = stack_size; // updating in the Thread class the stack_size variable, assumes its valid.
    // create array to hanke the available thread numbers.

    // Install timer_handler as the signal handler for SIGVTALRM.
    struct sigaction sa = {};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = &Scheduler::_timer_handler;

    if (sigaction(SIGVTALRM, &sa, nullptr))
    {
        SYS_ERROR("Failed to define SIGVTALRM handler during init.");
        exit(EXIT_FAILURE);
    }


    // init MAIN thread
    Thread* main_thread;
    try{
        main_thread = new Thread();
    }catch (bad_alloc& ){
        SYS_ERROR("Allocation of main thread failed.");
        exit(EXIT_FAILURE);
    }

    // add the main thread to the scheduler and define it as _running_thread.
    _tid_to_thread_map[MAIN_THREAD_ID] = main_thread;
    _running_thread = MAIN_THREAD_ID;

    // init the quantums to include the next quantum.
    main_thread->_total_running_quantums = SCHED_TOTAL_RUNNING_QUANTUMS_INIT;

    // init the timer.
    struct itimerval timer = { {TIMER_SECONDS_INIT, _quantum_usecs}, {TIMER_SECONDS_INIT, _quantum_usecs}};
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))  {
        SYS_ERROR("Failed to define 'setitimer' with ITIMER_VIRTUAL");
        exit(EXIT_FAILURE);
    }

}


void Scheduler::_timer_handler(int sig)
{

    if (sig != SIGVTALRM){ // validating the call use.
        string msg = "Used the SIGVTALRM handler for " + to_string(sig) + " signal number";
        LIB_ERROR(msg)
        return;
    }

    auto scheduler = get_instance();

    // update the sleeping threads to decrement the last quantum.
    scheduler->_update_sleeping();

    // get the current running thread object (prev_thread).
    Thread* prev_thread = scheduler->_tid_to_thread_map[scheduler->_running_thread];

    // update total running quantum, its include the next quantum (of the next thread to be running).
    scheduler->_total_running_quantums += 1;

    if(!scheduler->_ready.empty()){
        // get first in ready as next_tid
        int next_tid = scheduler->_ready.front();

        // removes next tid from ready.
        scheduler->_ready.pop_front();

        // push running to end of ready and update running to next_tid.
        scheduler->_ready.push_back(prev_thread->_tid);

        scheduler->_running_thread = next_tid;
        // get the next thread object. to set it running.
        Thread *next_thread = scheduler->_tid_to_thread_map[next_tid];

        // update its _total_running_quantums to include the next quantum.
        // scheduler->_total_running_quantums already updated above.
        next_thread->_total_running_quantums += 1;

        // set current running: to be next_tid
        if (sigsetjmp(prev_thread->_env, SAVE_MASK) == CURRENTLY_SET_UP){

            // jump to the next tid.
            siglongjmp(next_thread->_env, SIGLONGJMP_RETURN_VALUE);
        }
        // returned from jmp.

    } else {
        // else: ready is empty -> the main is running.
        // keep run it.
        // update its _total_running_quantums to include the next quantum.
        // scheduler->_total_running_quantums already updated above.
        prev_thread->_total_running_quantums += 1;
        // return to main thread run.
    }
}


void Scheduler::init(int quantum_usecs, int stack_size, int max_thread_num) {
    /**
     * This is a safer way to create an instance. instance = new Singleton is
     * dangeruous in case two instance threads wants to access at the same time
     */

    if(!Scheduler::_singleton_scheduler){ // check if the scheduler is nullptr (not initiated yet).
        try{
            Scheduler::_singleton_scheduler = std::unique_ptr<Scheduler>(new Scheduler(quantum_usecs, stack_size,  max_thread_num));
            return;

        }catch (bad_alloc& exp){
            SYS_ERROR("Allocation of scheduler failed.");
            exit(EXIT_FAILURE);
        }
    }
    // according to assumptions (in uthread_init) shouldn't happen.
    LIB_ERROR("The library already initiated.");
}


Scheduler *Scheduler::get_instance() {

    if(Scheduler::_singleton_scheduler == nullptr){
        // according to assumptions (in uthread_init) shouldn't happen.
        LIB_ERROR("trying to access the scheduler, but the library has not yet been initiated.");
        return nullptr;
    }
    return Scheduler::_singleton_scheduler.get();
}

int Scheduler::spawn(thread_entry_point entry_point) {

    if (entry_point == nullptr){
        LIB_ERROR("trying to spawn thread, but got nullptr as the thread's function.");
        return FAILURE;
    }

    if ((int)_tid_to_thread_map.size() == _max_thread_num){
        LIB_ERROR("trying to spawn new thread id, but reached the maximal available running threads.")
        return NO_AVAILABLE_TID;
    }

    int new_thread_tid = _get_next_tid();

    try{
        auto *new_thread = new Thread(new_thread_tid, entry_point);

        _ready.push_back(new_thread_tid);
        _tid_to_thread_map[new_thread_tid] = new_thread;

    }catch (bad_alloc& ){
        SYS_ERROR("Failed to allocate thread.")
        exit(EXIT_FAILURE);
    }

    return new_thread_tid;
}


int Scheduler::_get_next_tid() {

    for( int i = 1; i < _max_thread_num; i++){
        if(_tid_to_thread_map.find(i) == _tid_to_thread_map.end()){
            // available tid - not in the _tid_to_thread_map keys.
            return i;
        }
    }
    // should be not reachable.
    LIB_ERROR("Maximal number of threads spawend.");
    return NO_AVAILABLE_TID;
}

Scheduler::~Scheduler() {

    for (auto kv : _tid_to_thread_map){
        delete _tid_to_thread_map[kv.first]; //free the thread
        _tid_to_thread_map[kv.first] = nullptr;
    }
}

int Scheduler::get_total_quantums() const {
    return _total_running_quantums;
}

int Scheduler::get_thread_quantums(int tid) {
    auto it = _tid_to_thread_map.find(tid);

    if(it == _tid_to_thread_map.end()){
        string msg = "Trying to get the number of runnning quantums of thread number " + to_string(tid) + " that not exists";
        LIB_ERROR(msg);
        return FAILURE;
    }
    return it->second->_total_running_quantums;
}

int Scheduler::get_tid() const {
    return _running_thread;
}

int Scheduler::terminate_tid(int tid) {

    if (tid == MAIN_THREAD_ID){
        // no need to update the total running quantums OR anything else, will free the resources with Scheduler dctor.
        exit(SUCCESS);
    }

    // checks if tid is not valid (not between 1 to MAX_THREAD)
    // OR if the tid is not taken (there is no thread with this tid).
    if ((tid < MAIN_THREAD_ID) | (_max_thread_num <= tid) | (_tid_to_thread_map.find(tid) == _tid_to_thread_map.end())){
        string msg = "Trying to terminate thread number " + to_string(tid) + " but the thread not exists.";
        LIB_ERROR(msg);
        return FAILURE;
    }

    // tid exists.
    _remove_thread_from_scheduler(tid);

    // check if tid its of current running thread -> the running thread terminate itself.
    if (tid == _running_thread){
        // no need to save current _env, its terminated.
        // need to jmp to the next in _ready queue.
        _run_first_in_ready();  // will not return from here -> jmps to first in ready
    }

    // else: kill other thread -> can return to running thread with success.
    return SUCCESS;
}

void Scheduler::_remove_thread_from_scheduler(int tid) {

    if (tid != _running_thread){
        // its not the running thread, can be in the scheduler data structures.

        // need to remove from _ready.
        if(!_ready.empty()){ // can be empty if all other thread other than main (that running) in block/sleep.
            for (auto iter = _ready.begin(); iter != _ready.end(); iter++){
                if (*iter == tid){
                    _ready.erase(iter);
                    break;
                }
            }
        }

        // need to remove from _blocking.
        _blocked_threads_tid.erase(tid); // will remove if exists.

        // need to remove from _sleeping.
        _sleeping_tid_to_remaining_quantums.erase(tid); // will remove if exists.
    }

    delete _tid_to_thread_map[tid]; // free the thread resources.
    _tid_to_thread_map[tid] = nullptr;

    _tid_to_thread_map.erase(tid); // remove from scheduler threads map.

}

int Scheduler::block_tid(int tid) {
    // check if trying to block the main thread
    if (tid == MAIN_THREAD_ID){
        LIB_ERROR("Trying to block the main thread.")
        return FAILURE;
    }

    // validate that the thread exists.
    if(!_tid_exists(tid)){
        string msg = "Trying to block thread num " + to_string(tid) + " that not exist ";
        LIB_ERROR(msg);
        return FAILURE;
    }

    // check if the thread already blocked
    if(_blocked_threads_tid.find(tid) != _blocked_threads_tid.end()){
        // already blocked - nothing to do
        return SUCCESS;
    }

    // insert the thread to the blocked_threads
    _blocked_threads_tid.insert(tid);

    // check if the running thread trying to block himself.
    if (_running_thread == tid){
        // doing scheduling decision, save running thread _env to allow jmp back when resumed.
        if(sigsetjmp(_tid_to_thread_map[tid]->_env, SAVE_MASK) == CURRENTLY_SET_UP){
            _run_first_in_ready(); // will not return from here -> jmps to first in ready
        }

        // returned from siglongjmp

    }else{
        // not the running thread and also not in blocked threads, need to remove it from the ready deque.

        _remove_tid_from_ready(tid);
    }

    return SUCCESS;
}

void Scheduler::_run_first_in_ready() {

    // get first in ready as next_tid
    int next_tid = _ready.front(); // can't be empty because the main has to be there.

    // removes next tid from ready.
    _ready.pop_front();

    // no need to push running to end of ready. update running to be next_tid.
    _running_thread = next_tid;

    // get the next thread object. to set it running.
    Thread *next_thread = _tid_to_thread_map[next_tid];

    // update its running quantums to include the next quantum.
    next_thread->_total_running_quantums += 1;

    // update the scheduler _total_running_quantums to include the next quantum -> jumping to next_tid.
    _total_running_quantums += 1;

    // init timer and jump to it.
    INIT_TIMER
    siglongjmp(next_thread->_env, 1);
}

void Scheduler::_remove_tid_from_ready(int tid) {
    for (auto iter = _ready.begin(); iter != _ready.end() ; ++iter)
        if(*iter == tid) {
            _ready.erase(iter);
            return;
        }
}

int Scheduler::resume_tid(int tid) {

    // validate that the thread exists.
    if(!_tid_exists(tid)){
        string msg = "Trying to resume thread num " + to_string(tid) + " that not exist ";
        LIB_ERROR(msg);
        return FAILURE;
    }
    auto it_blocked = _blocked_threads_tid.find(tid);

    if (it_blocked == _blocked_threads_tid.end()){
        // the thread is not in blocked state, nothing to do.
        return SUCCESS;
    }

    // the thread in blocked -> remove it from block.
    _blocked_threads_tid.erase(it_blocked);

    // check if thread in sleeping, if yes -> need to wait till the sleep ends -> can't go to ready.
    auto it_sleeping = _sleeping_tid_to_remaining_quantums.find(tid);

    if (it_sleeping == _sleeping_tid_to_remaining_quantums.end()){
        // the thread not in sleeping state
        // -> insert it to the ready

        _ready.push_back(tid);
    } // in sleeping -> skip inserting to _ready.

    return SUCCESS;
}

bool Scheduler::_tid_exists(int tid) {
    return _tid_to_thread_map.find(tid) != _tid_to_thread_map.end();
}

int Scheduler::sleep_running(int num_quantums) {
    /**
     * Assumes num_quantums > 0.
     * */
    // check if the running is the main, which can't set himself tp sleep.
    if(_running_thread == MAIN_THREAD_ID){
        LIB_ERROR("Trying to set the main thread to sleep");
        return FAILURE;
    }

    _update_sleeping();
    // insert the _running_thread to sleep for the next 'num_quantums'.
    _sleeping_tid_to_remaining_quantums[_running_thread] = num_quantums;

    // save current going to sleep thread env and jmp to the next in ready
    if (sigsetjmp(_tid_to_thread_map[_running_thread]->_env, SAVE_MASK) == CURRENTLY_SET_UP){
        _run_first_in_ready(); // run the next thread in ready - will not return.
    }
    // sleeping thread returnd from siglongjmp.

    return SUCCESS; // not reachable.
}

void Scheduler::_update_sleeping() {

    std::vector<int> ends_its_sleeping_period;
    for(auto&& pair : _sleeping_tid_to_remaining_quantums)
    {
        int cur_tid = pair.first;
        int cur_tid_remaining_quantums = pair.second;

        if (pair.second == PREV_QUANTUM_WAS_THE_LAST){
            // it was the last quantum for cur_tid, need to remove him from _sleeping_tid_to_remaining_quantums
            // also need to check if it is not blocked, if not move to ready.

            // add the tid to the need to be removed from sleeping vector.
            ends_its_sleeping_period.push_back(cur_tid);

            // checks if the tid not in block, if not -> push it to the ready deque.
            if(_blocked_threads_tid.find(cur_tid) == _blocked_threads_tid.end()){
                _ready.push_back(cur_tid);
            }
            // else: its blocked, will be removed from sleep,
            // need to wait to the end of the block before moving to the ready.
        }else{
            _sleeping_tid_to_remaining_quantums[cur_tid] = cur_tid_remaining_quantums - 1; // decrement the last quantum.
        }

    }

    // remove all the threads that the previous quantum was there lsat sleeping quantum.
    for(auto&& key : ends_its_sleeping_period)
        _sleeping_tid_to_remaining_quantums.erase(key);
}

