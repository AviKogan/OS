

#include <iostream>
#include "signalMasker.h"

#define SYS_ERROR(text) std::cerr << "system error: " << (text) << std::endl;


SigMasker::SigMasker(int sig) {

    if(sigemptyset(&_set)){
        SYS_ERROR("Failed in sigemptyset");
        exit(EXIT_FAILURE);
    };


    if(sigaddset(&_set, sig)) {
        SYS_ERROR("Failed in sigaddset");
        exit(EXIT_FAILURE);
    }

    if(sigprocmask(SIG_BLOCK, &_set, nullptr)){
        SYS_ERROR("Failed in sigprocmask");
        exit(EXIT_FAILURE);
    }
}

SigMasker::~SigMasker() {

    if(sigprocmask(SIG_UNBLOCK, &_set, nullptr)){
        SYS_ERROR("Failed in sigprocmask");
        exit(EXIT_FAILURE);
    }

}

