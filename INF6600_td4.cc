#include <cstdlib>
#include <iostream>

#include <pthread.h>
#include <mqueue.h>
#include <string>
#include <errno.h>

#include "Patient.h"
#include "Message.h"

struct Data {
    Patient *patient;
    MQHandler *mqHandler;
};

void *t_controller(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;

    for (int i = 0; i < 40; i++) {
        sleep(0.1);
        double glycemia = patient->computeGlycemia();
        std::cout <<  "Glycemia : " << glycemia << std::endl;

        if (glycemia <= patient->glycemia_crit) {
            Message msg_glucose = START;
            Message msg_insuline = STOP;
            if(mq_send(mqHandler->qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), 0) < 0)
                std::cout << "Error sending glucose msg" << std::endl;
            if(mq_send(mqHandler->qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), 0) < 0)
                std::cout << "Error sending insuline msg" << std::endl;
        } else if (glycemia >= patient->glycemia_ref) {
            Message msg_glucose = STOP;
            Message msg_insuline = START;
            if(mq_send(mqHandler->qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), 0) < 0)
                std::cout << "Error sending glucose msg" << std::endl;
            if(mq_send(mqHandler->qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), 0) < 0)
                std::cout << "Error sending insuline  msg" << std::endl;
        }
    }

    Message msg = HALT;
    if(mq_send(mqHandler->qw_glucose, (const char *) &msg,
                sizeof(msg), 0) < 0)
        std::cout << "Error sending glucose msg halt" << std::endl;
    if(mq_send(mqHandler->qw_insuline, (const char *) &msg,
                sizeof(msg), 0) < 0)
        std::cout << "Error sending glucose msg halt" << std::endl;
    if(mq_send(mqHandler->qw_display, (const char *) &msg,
                sizeof(msg), 0) < 0)
        std::cout << "Error sending display msg halt" << std::endl;

    pthread_exit(NULL);
}

void *t_glucose(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    bool isInjecting = false;

    while (true) {
        sleep(0.1);
        Message msg = NONE;
        mq_attr attr, old_attr;
        mq_getattr(mqHandler->qr_glucose, &attr);
        if (attr.mq_curmsgs != 0) {
            // First set the queue to not block any calls
            attr.mq_flags = O_NONBLOCK;
            mq_setattr(mqHandler->qr_glucose, &attr, &old_attr);
            // Now consume all of the messages. Only the last one is usefull
            while (mq_receive(mqHandler->qr_glucose, (char *) &msg, MSG_SIZE, NULL) != -1)
            {}
            if (errno == EAGAIN)
                std::cout << "No more messages in glucose queue" << std::endl;
            else
                std::cout << "Error receiving glucose msg" << std::endl;

            // Now restore the attributes
            mq_setattr(mqHandler->qr_glucose, &old_attr, NULL);
        }

        if (msg== START) {
            isInjecting = true;
            std::cout << "Glucose : Msg start" << std::endl;
        } else if (msg == STOP) {
            isInjecting = false;
            std::cout << "Glucose : Msg stop" << std::endl;
        } else if (msg == HALT) {
            std::cout << "Glucose : Msg halt" << std::endl;
            pthread_exit(NULL);
        }

        if (isInjecting)
            std::cout << "Glucose : Injection" << std::endl;
            patient->injectGlucose();
    }
}

void *t_insuline(void *args) {
    Data *data = (Data *) args;
    Patient *patient = data->patient;
    MQHandler *mqHandler = data->mqHandler;
    bool isInjecting = true;

    while (true) {
        sleep(0.1);
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
            if (errno == EAGAIN)
                std::cout << "No more messages in insuline queue" << std::endl;
            else
                std::cout << "Error receiving insuline msg" << std::endl;

            // Now restore the attributes
            mq_setattr (mqHandler->qr_insuline, &old_attr, NULL);
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
        if (mq_receive(mqHandler->qr_display, (char *) &msg, MSG_SIZE, NULL) == -1) {
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
            case START:
            case STOP:
                break;
        }
        std::cout << msgText << std::endl;

        if (msg == HALT) {
            pthread_exit(NULL);
        }
    }
}

int main(int argc, char **argv) {

    Patient patient;
    MQHandler mqHandler;
    Data data = {&patient, &mqHandler};

    pthread_t th_controller;
    pthread_create(&th_controller, NULL, t_controller, &data);

    pthread_t th_glucose;
    pthread_create(&th_glucose, NULL, t_glucose, &data);

    pthread_t th_insuline;
    pthread_create(&th_insuline, NULL, t_insuline, &data);

    pthread_t th_display;
    pthread_create(&th_display, NULL, t_display, &data);

    pthread_join(th_controller, NULL);
    pthread_join(th_glucose, NULL);
    pthread_join(th_insuline, NULL);
    pthread_join(th_display, NULL);

    pthread_exit(NULL);
}
