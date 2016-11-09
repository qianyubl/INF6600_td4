
#ifndef SYRINGE_H
#define SYRINGE_H

#include <pthread.h>

class Syringe {
public:
    Syringe() : s_active(0) {
        s_level[0] = 10;
        s_level[1] = 100;
        pthread_mutex_init(&m_syringe, NULL);
    }

    ~Syringe() {
        pthread_mutex_destroy(&m_syringe);
    }

    void pump() {
        s_level[s_active] -= s_step;
    }

    double inspect() {
        return s_level[s_active];
    }

    int getActiveSyringe() {
        return s_active;
    }

    void syringeSwitch() {
        pthread_mutex_lock(&m_syringe);
        s_active = 1 - s_active;
        pthread_mutex_unlock(&m_syringe);
    }

    void reset() {
        pthread_mutex_lock(&m_syringe);
        s_level[1-s_active] = 100;
        pthread_mutex_unlock(&m_syringe);
    }

    void stop() {
        s_level[0] = -1;
        s_level[1] = -1;
    }

    static const double level_critical = 1;
    static const double level_weak = 5;
    static const double s_step = 2;

    pthread_mutex_t m_syringe;

private:
    double s_level[2];
    int s_active;
};

#endif
