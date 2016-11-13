#include <cstdlib>
#include <iostream>

#include <time.h>
#include <pthread.h>
#include <mqueue.h>
#include <string>
#include <errno.h>

#include "Error.h"
#include "Patient.h"
#include "Message.h"
#include "Syringe.h"

#define EXECUTION_TIME 5*60
#define CYCLE_TIME 0.5
#define EXECUTION_CYCLE EXECUTION_TIME / CYCLE_TIME
#define FACTOR_TIME 0.1

// setting the priority numbers
enum Priority {
    VERY_CRITICAL = 20,
    CRITICAL = 18,
    VERY_URGENT = 15,
    URGENT = 13,
    NORMAL = 10,
    WEAK = 5,
};

// define the data structure
struct Data {
    Patient *patient;
    MQHandler *mqHandler;
    Syringe *sManager;
};

// controller task
void *t_controller(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    Syringe *sManager = data->sManager;

    for (int i = 0; i < EXECUTION_CYCLE; ++i) {
        usleep(1000000 * CYCLE_TIME * FACTOR_TIME);
	// call the glycemia module
        double glycemia = patient->computeGlycemia();
	// case of critical glycemia
	// add messages in glucose, insuline and display queues 
        if (glycemia <= patient->glycemia_crit) {
            Message msg_glucose = START;
            Message msg_insuline = STOP;
            Message msg_critical = GLYCEMIA_CRITICAL;
            int r = mq_send(mqHandler->qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), CRITICAL);
            CHECK(r >= 0, "Error sending glucose msg");
            r = mq_send(mqHandler->qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), CRITICAL);
            CHECK(r >= 0, "Error sending insuline msg");

            r = mq_send(mqHandler->qw_display, (const char *) &msg_critical,
                        sizeof(msg_critical), CRITICAL);
            CHECK(r >= 0, "Error sending display msg");
        } else if (glycemia >= patient->glycemia_ref) {
	// case of normal glycemia
	// add messages in glucose, insuline and display queues 
            Message msg_glucose = STOP;
            Message msg_insuline = START;
            Message msg_normal = GLYCEMIA_NORMAL;
            int r = mq_send(mqHandler->qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), URGENT);
            CHECK(r >= 0, "Error sending glucose msg");

            r = mq_send(mqHandler->qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), URGENT);
            CHECK(r >= 0, "Error sending insuline  msg");

            r = mq_send(mqHandler->qw_display, (const char *) &msg_normal,
                        sizeof(msg_normal), NORMAL);
            CHECK(r >= 0, "Error sending display msg");
        }
    }

    // at simulation end send halt message in the queue q_glucose, q_display, q_insuline
    Message msg = HALT;
    int r = mq_send(mqHandler->qw_glucose, (const char *) &msg,
                sizeof(msg), NORMAL);
    CHECK(r >= 0, "Error sending glucose msg halt");
    r = mq_send(mqHandler->qw_insuline, (const char *) &msg,
                sizeof(msg), NORMAL);
    CHECK(r >= 0, "Error sending glucose msg halt");
    r = mq_send(mqHandler->qw_display, (const char *) &msg,
                sizeof(msg), NORMAL);
    CHECK(r >= 0, "Error sending display msg halt");

    // call stop method to stop the syringe usage
    pthread_mutex_lock(&sManager->m_syringe);
    sManager->stop();
    pthread_cond_signal(&mqHandler->cv_syringe);
    pthread_mutex_unlock(&sManager->m_syringe);

    pthread_exit(NULL);
}

// task in charge of injecting glucose
void *t_glucose(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    bool isInjecting = false;

    while (true) {
        usleep(1000000 * CYCLE_TIME * FACTOR_TIME);
        Message msg = NONE;
        mq_attr attr, old_attr;
        mq_getattr(mqHandler->qr_glucose, &attr);
        if (attr.mq_curmsgs != 0) {
            // First set the queue to not block any calls
            attr.mq_flags = O_NONBLOCK;
            mq_setattr(mqHandler->qr_glucose, &attr, &old_attr);
            // Now consume all of the messages. Only the last one is usefull
            while (mq_receive(mqHandler->qr_glucose,
                        (char *) &msg, MSG_SIZE, NULL) != -1) {}
            CHECK(errno == EAGAIN, "Error receiving glucose msg");

            // Now restore the attributes
            mq_setattr(mqHandler->qr_glucose, &old_attr, NULL);
        }
	// set the variable is injecting to start the injection 		  
	// and add a message in q_display_queue     
        if (msg == START) {
            isInjecting = true;
            Message msg = GLUCOSE_START;
            int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                        sizeof(msg), URGENT);
            CHECK(r >= 0, "Error sending display msg");
        } else if (msg == STOP) {
	// add a message in q_display_queue to indiquate the glucose injection stop     
            Message msg = GLUCOSE_STOP;
            int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                        sizeof(msg), NORMAL);
            CHECK(r >= 0, "Error sending display msg");
            isInjecting = false;
        } else if (msg == HALT) {     
            pthread_exit(NULL);
        }

        if (isInjecting) {
	// glucose injection     
            patient->injectGlucose();
        }
    }
}

// task in charge of injecting insuline
void *t_insuline(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    Syringe *sManager = data->sManager;
    bool isInjecting = true;

    while (true) {
        usleep(1000000 * CYCLE_TIME * FACTOR_TIME);
        Message msg = NONE;

        mq_attr attr, old_attr;
        mq_getattr (mqHandler->qr_insuline, &attr);
        if (attr.mq_curmsgs != 0) {
            attr.mq_flags = O_NONBLOCK;
            mq_setattr(mqHandler->qr_insuline, &attr, &old_attr);
            while (mq_receive(mqHandler->qr_insuline,
                        (char *) &msg, MSG_SIZE, NULL) != -1) {}
            CHECK(errno == EAGAIN, "Error receiving insuline msg");

            mq_setattr(mqHandler->qr_insuline, &old_attr, NULL);
        }
	  
        if (msg == START) {
	   // set the variable is injecting to start the injection 		  
	   // and add a message in q_display_queue     
            isInjecting = true;
            Message msg = INSULINE_START;
            int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                        sizeof(msg), URGENT);
            CHECK(r >= 0, "Error sending display msg");
        } else if (msg == STOP) {
	    // add a message in q_display_queue     
            Message msg = INSULINE_STOP;
            int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                        sizeof(msg), NORMAL);
            CHECK(r >= 0, "Error sending display msg");
            isInjecting = false;
        } else if (msg == HALT) {
            pthread_exit(NULL);
        }

        if (isInjecting) {
	    // pump the insuline solution
            // this action need to be protected by a mutex as 
	    // s_level variable is shared with t_syringe task
            pthread_mutex_lock(&sManager->m_syringe);
            sManager->pump();
            // send a signal that indiquate that syringe level is changing
            pthread_cond_signal(&mqHandler->cv_syringe);
            pthread_mutex_unlock(&sManager->m_syringe);
	    // pump the insuline solution
            patient->injectInsuline();
        }
    }
}

// task responsable of displaying the alerts and informations 
// messages found in qr_display queue
void *t_display(void *args) {
    Data *data = (Data *) args;
    MQHandler *mqHandler = data->mqHandler;

    while (true) {
        Message msg = NONE;
        if (mq_receive(mqHandler->qr_display,
                    (char *) &msg, MSG_SIZE, NULL) == -1)
        {
            std::cerr << "Error receiving display msg" << std::endl;
            continue;
        }
        std::string msgText;
        switch(msg) {
            case HALT:
                msgText = "Stopping the system";
                break;
            case GLYCEMIA_CRITICAL:
                msgText = "Glycemia critical";
                break;
            case GLYCEMIA_NORMAL:
                msgText = "Glycemia normal";
                break;
            case GLUCOSE_START:
                msgText = "Start glucose injection";
                break;
            case GLUCOSE_STOP:
                msgText = "Stop glucose injection";
                break;
            case INSULINE_START:
                msgText = "Start insuline injection";
                break;
            case INSULINE_STOP:
                msgText = "Stop insuline injection";
                break;
            case ANTIBIO_INJECT:
                msgText = "Antibiotic injection";
                break;
            case ANTICOAG_INJECT:
                msgText = "Antiocoagulant injection";
                break;
            case SYRINGE_1_LOW:
                msgText = "Solution level in syringe 1 reaches 5%";
                break;
            case SYRINGE_2_LOW:
                msgText = "Solution level in syringe 2 reaches 5%";
                break;
            case SYRINGE_1_CRITICAL:
                msgText = "Solution level in syringe 1 reaches 1%";
                break;
            case SYRINGE_2_CRITICAL:
                msgText = "Solution level in syringe 2 reaches 1%";
                break;
            case SWITCH:
                msgText = "Switch between syringe";
                break;
            case RESET:
                msgText = "Reset inactive syringe";
                break;
            case START:
            case STOP:
            case NONE:
                break;
        }
        std::cout << msgText << std::endl;

        if (msg == HALT) {
            pthread_exit(NULL);
        }
    }
}

// task in charge of the syringe reset and switch
void *t_syringe(void *args) {

    Data *data = (Data *) args;
    MQHandler *mqHandler = data->mqHandler;
    Syringe *sManager = data->sManager;

    while (true) {
	// waiting for condvar signal indiquating that the current syringe level changed	
        pthread_mutex_lock(&sManager->m_syringe);
        pthread_cond_wait(&mqHandler->cv_syringe, &sManager->m_syringe);
        // read the shared variables s_level and s_activate
        double level = sManager->inspect();
        int s_active = sManager->getActiveSyringe();
        pthread_mutex_unlock(&sManager->m_syringe);

        Message msg = NONE;

        if (level < 0) {
	    // stop the thread
            pthread_exit(NULL);
        } else if (level == Syringe::level_critical) {
            // switch and reset syringe when level reach 1%
            if (s_active == 0)
                msg = SYRINGE_1_CRITICAL;
            else
                msg = SYRINGE_2_CRITICAL;

            sManager->syringeSwitch();
            sManager->reset();
	    // add message in q_display queue indicating that syringe level reach 1%
            int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                    sizeof(msg), URGENT);
            CHECK(r >= 0, "Error sending syringe level msg");
        } else if (level == Syringe::level_weak) {
            if (s_active == 0)
                msg = SYRINGE_1_LOW;
            else
                msg = SYRINGE_2_LOW;
	    // add message in q_display queue indicating that syringe level reach 5%
            int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                    sizeof(msg), NORMAL);
            CHECK(r >= 0, "Error sending syringe level msg");
        }
    }
}

// add a message ANTIBIO_INJECT in the display queue 
void t_antibio(sigval args) {
    Data *data = (Data *) args.sival_ptr;
    MQHandler *mqHandler = data->mqHandler;

    Message msg = ANTIBIO_INJECT;
    int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                sizeof(msg), WEAK);
    CHECK(r >= 0, "Error sending display msg ANTIBIO_INJECT");
}

// add a message ANTICOAG_INJECT in the display queue 
void t_anticoag(sigval args) {
    Data *data = (Data *) args.sival_ptr;
    MQHandler *mqHandler = data->mqHandler;

    Message msg = ANTICOAG_INJECT;
    int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                sizeof(msg), WEAK);
    CHECK(r >= 0, "Error sending display msg ANTICOAG_INJECT");
}

int main(int argc, char **argv) {

    // create data structure and instantiate the classes
    // Patient, MQHandler, Syringe
    Patient patient;
    MQHandler mqHandler;
    Syringe sManager;
    Data data = {&patient, &mqHandler, &sManager};

    sched_param s_param;
    pthread_attr_t attr;
    setprio(0, 20);
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	
    // create the thread th_controller with most high priority
    pthread_t th_controller;
    s_param.sched_priority = VERY_CRITICAL;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_controller, &attr, t_controller, &data);

    // create the thread th_syringe with critical  priority
    pthread_t th_syringe;
    s_param.sched_priority = CRITICAL;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_controller, &attr, t_syringe, &data);

    // create the thread th_glucose with very urgent priority
    pthread_t th_glucose;
    s_param.sched_priority = VERY_URGENT;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_glucose, &attr, t_glucose, &data);

    // create the thread th_insuline with urgent priority
    pthread_t th_insuline;
    s_param.sched_priority = URGENT;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_insuline, &attr, t_insuline, &data);

    // create the thread th_display with normal priority
    pthread_t th_display;
    s_param.sched_priority = NORMAL;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_display, &attr, t_display, &data);

    // create timer and sigevent to simulate the period task antibiotic injection
    timer_t timerAntibioId;
    itimerspec timerAntibio;
    sigevent eventAntibio;

    SIGEV_THREAD_INIT(&eventAntibio, t_antibio, &data, NULL);
    timer_create(CLOCK_REALTIME, &eventAntibio, &timerAntibioId);
	
    // set the start time for the timer
    timerAntibio.it_value.tv_sec = clock_t(130 * FACTOR_TIME);
    timerAntibio.it_value.tv_nsec= 0;
    // set the timer period
    timerAntibio.it_interval.tv_sec = clock_t(4*3600 * FACTOR_TIME);
    timerAntibio.it_interval.tv_nsec = 0;

    timer_settime(timerAntibioId, 0, &timerAntibio, NULL);

    // create timer and sigevent to simulate the period task anticoagulant injection
    timer_t timerAnticoagId;
    itimerspec timerAnticoag;
    sigevent eventAnticoag;

    SIGEV_THREAD_INIT(&eventAnticoag, t_anticoag, &data, NULL);
    timer_create(CLOCK_REALTIME, &eventAnticoag, &timerAnticoagId);

    // set the start time for the timer
    timerAnticoag.it_value.tv_sec = clock_t(10 * FACTOR_TIME);
    timerAnticoag.it_value.tv_nsec= 0;
    // set the timer period
    timerAnticoag.it_interval.tv_sec = clock_t(24*3600 * FACTOR_TIME);
    timerAnticoag.it_interval.tv_nsec = 0;

    timer_settime(timerAnticoagId, 0, &timerAnticoag, NULL);

    // join all the thread
    pthread_join(th_controller, NULL);
    pthread_join(th_syringe, NULL);
    pthread_join(th_glucose, NULL);
    pthread_join(th_insuline, NULL);
    pthread_join(th_display, NULL);

    pthread_exit(NULL);
}
