
#include "thread.h"

// --------------- used as blackbox -----------------//
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

// ----------------------------------------------//

Thread::Thread(int tid, thread_entry_point entry_point): _tid(tid){

    try{
        _stack = new char[stack_size];
        // setup_thread
        address_t sp = (address_t) _stack + stack_size - sizeof(address_t);
        address_t pc = (address_t) entry_point;

        if(sigsetjmp(_env, 1)){
            SYS_ERROR("Failed in sigsetjmp during Thread init.")
            exit(EXIT_FAILURE);
        }

        (_env->__jmpbuf)[JB_SP] = translate_address(sp);
        (_env->__jmpbuf)[JB_PC] = translate_address(pc);

        if(sigemptyset(&_env->__saved_mask)){
            SYS_ERROR("Failed in sigemptyset during Thread init.")
            exit(EXIT_FAILURE);
        }

    }catch (std::bad_alloc &exp){
        SYS_ERROR("Failed to allocate thread");
        exit(EXIT_FAILURE);
    }
}

Thread::Thread():_tid(0){}

int Thread::stack_size = 0;

