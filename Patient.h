#ifndef PATIENT_H
#define PATIENT_H

#include <pthread.h>

class Patient {
    public:

        Patient()
        : glucose(100), insuline(0) {
            pthread_mutex_init(&m_glucose, NULL);
            pthread_mutex_init(&m_insuline, NULL);
        }

        ~Patient() {
            pthread_mutex_destroy(&m_glucose);
            pthread_mutex_destroy(&m_insuline);
        }

        static const double glycemia_ref = 120;
        static const double glycemia_crit = 60;
        static const int glucose_step = 10;
        static const int insuline_step = 30;
        static const int Kg = 1;
        static const int Ki = 1;

        double computeGlycemia() {
            pthread_mutex_lock(&m_glucose);
            pthread_mutex_lock(&m_insuline);
            double glycemia = Kg * glucose - Ki * insuline;
            pthread_mutex_unlock(&m_glucose);
            pthread_mutex_unlock(&m_insuline);

            return glycemia;
        }

        void injectGlucose() {
            pthread_mutex_lock(&m_glucose);
            glucose += glucose_step;
            pthread_mutex_unlock(&m_glucose);
        }

        void injectInsuline() {
            pthread_mutex_lock(&m_insuline);
            insuline += insuline_step;
            pthread_mutex_unlock(&m_insuline);
        }

        pthread_mutex_t m_glucose;
        pthread_mutex_t m_insuline;
    private:
        int glucose;
        int insuline;
};

#endif
