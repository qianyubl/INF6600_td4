#ifndef PATIENT_H
#define PATIENT_H

#include <pthread.h>
class Patient {
    public:

        Patient()
        : glucose(100), insuline(0) {
	// mutex m_glucose and m_insuline intialisation
	// the mutexes are used to insure the exclusive access to glucose and insuline parameters
            pthread_mutex_init(&m_glucose, NULL);
            pthread_mutex_init(&m_insuline, NULL);
        }

        ~Patient() {
	// mutex m_glucose and m_insuline destruction
            pthread_mutex_destroy(&m_glucose);
            pthread_mutex_destroy(&m_insuline);
        }
	// constants definitions
        static const double glycemia_ref = 120;
        static const double glycemia_crit = 60;
	// foreach glucose injection the quantity injected is glucose_step 
        static const int glucose_step = 1;
	// the quantity of insuline injected in each insuline_injection 
        static const int insuline_step = 1;
        static const double Kg = 1.6;
        static const double Ki = 1.36;

	// compute the new glycemia value using mutex for shared variables glucose and insuline
	// to protect the access to this variables  
        double computeGlycemia() {
            pthread_mutex_lock(&m_glucose);
            pthread_mutex_lock(&m_insuline);
            double glycemia = Kg * glucose - Ki * insuline;
            pthread_mutex_unlock(&m_glucose);
            pthread_mutex_unlock(&m_insuline);

            return glycemia;
        }
		
	// increment the glucose shared variable using mutex for shared variable glucose
	// to protect its access  
        void injectGlucose() {
            pthread_mutex_lock(&m_glucose);
            glucose += glucose_step;
            pthread_mutex_unlock(&m_glucose);
        }

	// increment the insuline shared variable using mutex for shared variable insuline
	// to protect its access  
        void injectInsuline() {
            pthread_mutex_lock(&m_insuline);
            insuline += insuline_step;
            pthread_mutex_unlock(&m_insuline);
        }
	// declaration of two mutex m_glucose and m_insuline
        pthread_mutex_t m_glucose;
        pthread_mutex_t m_insuline;

    private:
	// shared variables
        int glucose;
        int insuline;
};

#endif
