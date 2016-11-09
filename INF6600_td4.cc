#include <cstdlib>
#include <iostream>

#include <pthread.h>
#include <mqueue.h>
#include <string>
#include <errno.h>

#include "Error.h"
#include "Patient.h"
#include "Message.h"
#include "Syringe.h"

enum Priority {
    CRITICAL = 20,
    URGENT = 15,
    NORMAL = 10,
    WEAK = 5,
};

struct Data {
    Patient *patient;
    MQHandler *mqHandler;
    Syringe *sManager;
};

void *t_controller(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    Syringe *sManager = data->sManager;

    for (int i = 0; i < 40; i++) {
        usleep(100);
        double glycemia = patient->computeGlycemia();
        std::cout <<  "Glycemia : " << glycemia << std::endl;

        if (glycemia <= patient->glycemia_crit) {
            Message msg_glucose = START;
            Message msg_insuline = STOP;
            int r = mq_send(mqHandler->qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), CRITICAL);
            CHECK(r >= 0, "Error sending glucose msg");
            r = mq_send(mqHandler->qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), CRITICAL);
            CHECK(r >= 0, "Error sending insuline msg");
        } else if (glycemia >= patient->glycemia_ref) {
            Message msg_glucose = STOP;
            Message msg_insuline = START;
            int r = mq_send(mqHandler->qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), URGENT);
            CHECK(r >= 0, "Error sending glucose msg");
            r = mq_send(mqHandler->qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), URGENT);
            CHECK(r >= 0, "Error sending insuline  msg");
        }
    }

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

    pthread_mutex_lock(&sManager->m_syringe);
    sManager->stop();
    pthread_cond_signal(&mqHandler->cv_syringe);
    pthread_mutex_unlock(&sManager->m_syringe);

    pthread_exit(NULL);
}

void *t_glucose(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    bool isInjecting = false;

    while (true) {
        usleep(100);
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

        if (msg == START) {
            isInjecting = true;
            std::cout << "Glucose : Msg start" << std::endl;
        } else if (msg == STOP) {
            isInjecting = false;
            std::cout << "Glucose : Msg stop" << std::endl;
        } else if (msg == HALT) {
            std::cout << "Glucose : Msg halt" << std::endl;
            pthread_exit(NULL);
        }

        if (isInjecting) {
            std::cout << "Glucose : Injection" << std::endl;
            patient->injectGlucose();
        }
    }
}

void *t_insuline(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    Syringe *sManager = data->sManager;
    bool isInjecting = true;

    while (true) {
        usleep(100);
        Message msg = NONE;

        mq_attr attr, old_attr;
        mq_getattr (mqHandler->qr_insuline, &attr);
        if (attr.mq_curmsgs != 0) {
            // There are some messages on this queue....eat em
            // First set the queue to not block any calls
            attr.mq_flags = O_NONBLOCK;
            mq_setattr(mqHandler->qr_insuline, &attr, &old_attr);
            // Now eat all of the messages
            while (mq_receive(mqHandler->qr_insuline,
                        (char *) &msg, MSG_SIZE, NULL) != -1) {}
            CHECK(errno == EAGAIN, "Error receiving insuline msg");

            // Now restore the attributes
            mq_setattr(mqHandler->qr_insuline, &old_attr, NULL);
        }

        if (msg == START) {
            isInjecting = true;
            std::cout << "Insuline : Msg start" << std::endl;
        } else if (msg == STOP) {
            isInjecting = false;
            std::cout << "Insuline : Msg stop" << std::endl;
        } else if (msg == HALT) {
            std::cout << "Insuline : Msg halt" << std::endl;
            pthread_exit(NULL);
        }

        if (isInjecting) {
            pthread_mutex_lock(&sManager->m_syringe);
            sManager->pump();
            pthread_cond_signal(&mqHandler->cv_syringe);
            pthread_mutex_unlock(&sManager->m_syringe);
            patient->injectInsuline();
            std::cout << "Insuline : Injection" << std::endl;
        }
    }
}

void *t_display(void *args) {
    Data *data = (Data *) args;
    MQHandler *mqHandler = data->mqHandler;

    while (true) {
        Message msg = NONE;
        if (mq_receive(mqHandler->qr_display,
                    (char *) &msg, MSG_SIZE, NULL) == -1)
        {
            std::cout << "Error receiving display msg" << std::endl;
            continue;
        }
        std::string msgText;
        switch(msg) {
            case HALT:
                msgText = "Stopping the system...";
                break;
            case GLYCEMIA_CRITICAL:
                msgText = "Glycemia critical";
                break;
            case GLYCEMIA_NORMAL:
                msgText = "Glycemia normal";
                break;
            case ANTIBIO_INJECT:
                msgText = "Antibiotic injection";
                break;
            case ANTICOAG_INJECT:
                msgText = "Antiocoagulant injection";
                break;
            case SYRINGE_1_LOW:
                msgText = "Solution level in syringe 1 is low";
                break;
            case SYRINGE_2_LOW:
                msgText = "Solution level in syringe 2 is low";
                break;
            case SYRINGE_1_CRITICAL:
                msgText = "Solution level in syringe 1 is critical";
                break;
            case SYRINGE_2_CRITICAL:
                msgText = "Solution level in syringe 2 is critical";
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

void *t_syringe(void *args) {

    Data *data = (Data *) args;
    MQHandler *mqHandler = data->mqHandler;
    Syringe *sManager = data->sManager;

    while (true) {
        pthread_mutex_lock(&sManager->m_syringe);
        pthread_cond_wait(&mqHandler->cv_syringe, &sManager->m_syringe);
        double level = sManager->inspect();
        int s_active = sManager->getActiveSyringe();
        pthread_mutex_unlock(&sManager->m_syringe);

        Message msg = NONE;

        if (level < 0) {
            pthread_exit(NULL);
        } else if (level <= Syringe::level_critical) {
            // envoyer message
            if (s_active == 0)
                msg = SYRINGE_1_CRITICAL;
            else
                msg = SYRINGE_2_CRITICAL;

            sManager->syringeSwitch();
            sManager->reset();
        } else if (level <= Syringe::level_weak) {
            // envoyer message
            if (s_active == 0)
                msg = SYRINGE_1_LOW;
            else
                msg = SYRINGE_2_LOW;
        }

        if (msg != NONE) {
            int r = mq_send(mqHandler->qw_glucose, (const char *) &msg,
                        sizeof(msg), URGENT);
            CHECK(r >= 0, "Error sending syringe level msg");
        }
    }
}

void t_antibio(sigval args) {
    Data *data = (Data *) args.sival_ptr;
    MQHandler *mqHandler = data->mqHandler;

    Message msg = ANTIBIO_INJECT;
    int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                sizeof(msg), NORMAL);
    CHECK(r >= 0, "Error sending display msg ANTIBIO_INJECT");
}

void t_anticoag(sigval args) {
    Data *data = (Data *) args.sival_ptr;
    MQHandler *mqHandler = data->mqHandler;

    Message msg = ANTICOAG_INJECT;
    int r = mq_send(mqHandler->qw_display, (const char *) &msg,
                sizeof(msg), NORMAL);
    CHECK(r >= 0, "Error sending display msg ANTICOAG_INJECT");
}

int main(int argc, char **argv) {

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

    pthread_t th_controller;
    s_param.sched_priority = CRITICAL;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_controller, &attr, t_controller, &data);

    pthread_t th_syringe;
    s_param.sched_priority = CRITICAL;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_controller, &attr, t_syringe, &data);

    pthread_t th_glucose;
    s_param.sched_priority = URGENT;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_glucose, &attr, t_glucose, &data);

    pthread_t th_insuline;
    s_param.sched_priority = URGENT;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_insuline, &attr, t_insuline, &data);

    pthread_t th_display;
    s_param.sched_priority = NORMAL;
    pthread_attr_setschedparam(&attr, &s_param);
    pthread_create(&th_display, &attr, t_display, &data);

    timer_t timerAntibioId;
    itimerspec timerAntibio;
    sigevent eventAntibio;

    SIGEV_THREAD_INIT(&eventAntibio, t_antibio, &data, NULL);
    timer_create(CLOCK_REALTIME, &eventAntibio, &timerAntibioId);

    timerAntibio.it_value.tv_sec = 15;
    timerAntibio.it_value.tv_nsec= 0;
    timerAntibio.it_interval.tv_sec = 4*3600;
    timerAntibio.it_interval.tv_nsec = 0;

    timer_settime(timerAntibioId, 0, &timerAntibio, NULL);

    timer_t timerAnticoagId;
    itimerspec timerAnticoag;
    sigevent eventAnticoag;

    SIGEV_THREAD_INIT(&eventAnticoag, t_anticoag, &data, NULL);
    timer_create(CLOCK_REALTIME, &eventAnticoag, &timerAnticoagId);

    timerAnticoag.it_value.tv_sec = 5;
    timerAnticoag.it_value.tv_nsec= 0;
    timerAnticoag.it_interval.tv_sec = 24*3600;
    timerAnticoag.it_interval.tv_nsec = 0;

    timer_settime(timerAnticoagId, 0, &timerAnticoag, NULL);

    pthread_join(th_controller, NULL);
    pthread_join(th_syringe, NULL);
    pthread_join(th_glucose, NULL);
    pthread_join(th_insuline, NULL);
    pthread_join(th_display, NULL);

    pthread_exit(NULL);
}
