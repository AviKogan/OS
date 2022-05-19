//
// Created by avi on 05/04/2022.
//

#ifndef EX2_THREAD_H
#define EX2_THREAD_H

/*
 * sigsetjmp/siglongjmp demo program.
 * Hebrew University OS course.
 * Author: OS, os@cs.huji.ac.il
 */

#include <cstdio>
#include <csetjmp>
#include <csignal>
#include <unistd.h>
#include <sys/time.h>
#include <iostream>

#define SYS_ERROR(text) std::cerr << "system error: " << (text) << std::endl;

typedef void (*thread_entry_point)();


class Thread{
    /** The given thread id, assumes its valid*/
    int _tid;

    /** Store the environment of the thread*/
    sigjmp_buf _env;

    /** Allocated stack */
    char *_stack;

    /** */
    int _total_running_quantums;

    friend class Scheduler; // used to update local variable, such as _total_running_quantums, _env.

public:

    Thread(int tid, thread_entry_point entry_point);

    explicit Thread();

    ~Thread(){
        delete[] _stack;
        _stack = nullptr;
    }

    static int stack_size;
};

#endif //EX2_THREAD_H
