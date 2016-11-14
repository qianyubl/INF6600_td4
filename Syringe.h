
#ifndef SYRINGE_H
#define SYRINGE_H

#include <pthread.h>

class Syringe {
public:
    Syringe() : s_active(0) {
        // intialise the mutex m_syringe + the shared variables
        s_level[0] = 100;
        s_level[1] = 100;
        pthread_mutex_init(&m_syringe, NULL);
    }

    ~Syringe() {
        // destroy the mutex m_syringe
        pthread_mutex_destroy(&m_syringe);
    }

    // decrement the syringe level when pumping
    void pump() {
        s_level[s_active] -= s_step;
    }

    double inspect() {
        return s_level[s_active];
    }

    int getActiveSyringe() {
        return s_active;
    }

    // switch the syringe
    // as we are updating a shared variable, this action is protected by a mutex
    void syringeSwitch() {
        pthread_mutex_lock(&m_syringe);
        s_active = 1 - s_active;
        pthread_mutex_unlock(&m_syringe);
    }

    // reset the syringe not being used
    // as we are updating a shared variable, this action is protected by a mutex
    void reset() {
        pthread_mutex_lock(&m_syringe);
        s_level[1-s_active] = 100;
        pthread_mutex_unlock(&m_syringe);
    }

    // stop the solution injection
    void stop() {
        s_level[0] = -1;
        s_level[1] = -1;
    }

    // constants definitions
    static const double level_critical = 1;
    static const double level_weak = 5;
    // the quantity of solution used by the pump each time
    static const double s_step = 1;

    // declaration of m_syringe mutex to protect the access to shared variables
    // s_level and s_active
    pthread_mutex_t m_syringe;

private:
    // defintion of the shared variable s_level representing the syringe1/2
    // level
    double s_level[2];
    // store the syringe being used, 0 if syringe1, 1 if syringe2
    int s_active;
};

#endif
