
#include "uthreads.h"
#include "scheduler.h"
#include "signalMasker.h"


int uthread_init(int quantum_usecs) {

    if (quantum_usecs <= 0){
        LIB_ERROR("Illegal quantum time, got non-positive quantum time.")
        return FAILURE;
    }

    Scheduler::init(quantum_usecs, STACK_SIZE, MAX_THREAD_NUM);
    return SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point) {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->spawn(entry_point);
}

int uthread_terminate(int tid) {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->terminate_tid(tid);
}

int uthread_block(int tid) {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->block_tid(tid);
}

int uthread_resume(int tid) {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->resume_tid(tid);
}

int uthread_sleep(int num_quantums) {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->sleep_running(num_quantums);
}

int uthread_get_tid() {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->get_tid();
}

int uthread_get_total_quantums() {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->get_total_quantums();
}

int uthread_get_quantums(int tid) {
    SigMasker SIGVTALRM_masker = SigMasker(SIGVTALRM);
    return Scheduler::get_instance()->get_thread_quantums(tid);
}

