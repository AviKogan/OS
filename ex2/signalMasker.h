

#ifndef EX2_SIGNALMASKER_H
#define EX2_SIGNALMASKER_H

#include <csignal>

class SigMasker{
    sigset_t _set{};

public:

    explicit SigMasker(int sig);

    ~SigMasker();
};

#endif //EX2_SIGNALMASKER_H
