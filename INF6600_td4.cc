#include <cstdlib>
#include <iostream>

#include <pthread.h>
#include <mqueue.h>
#include <string.h>

#define MSG_SIZE 4096

mqd_t qw_glucose;
mqd_t qr_glucose;
mqd_t qw_insuline;
mqd_t qr_insuline;

class Patient {
    public:

        Patient() : glucose(0), insuline(0) {}

        pthread_mutex_t m_glucose;
        pthread_mutex_t m_insuline;
        int glucose;
        int insuline;

        static const double glycemia_ref = 120;
        static const double glycemia_crit = 60;
        static const int glucose_step = 1;
        static const int insuline_step = 3;
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
            glucose += glucose_step;
            pthread_mutex_unlock(&m_insuline);
        }
};

enum Message {STOP, START};

void *t_patient(void *args) {
    Patient *patient = (Patient *)args;

    for (int i = 0; i < 10; i++) {
        double glycemia = patient->computeGlycemia();
        std::cout <<  "Glycemia : " << glycemia << std::endl;

        if (glycemia <= patient->glycemia_crit) {
            Message msg_glucose = START;
            Message msg_insuline = STOP;
            if(mq_send(qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), 0) < 0)
                std::cout << "Error sending glucose msg" << std::endl;
            if(mq_send(qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), 0) < 0)
                std::cout << "Error sending glycemia  msg" << std::endl;
        } else if (glycemia >= patient->glycemia_ref) {
            Message msg_glucose = STOP;
            Message msg_insuline = START;
            if(mq_send(qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), 0) < 0)
                std::cout << "Error sending glucose msg" << std::endl;
            if(mq_send(qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), 0) < 0)
                std::cout << "Error sending glycemia  msg" << std::endl;
        }
    }

    pthread_exit(NULL);
}

void *t_glucose(void *args) {
    Patient *patient = (Patient *)args;
    bool isInjecting = false;

    for (int i = 0; i < 10; i++) {
        Message msg;
        int byte_read = mq_receive(qr_glucose, (char *) &msg, MSG_SIZE, NULL);
        if (byte_read < 0) {
            std::cout << "Error receiving glucose msg" << std::endl;
        } else if (msg== START) {
            isInjecting = true;
            std::cout << "Glucose : Msg start" << std::endl;
        } else if (msg == STOP) {
            isInjecting = false;
            std::cout << "Glucose : Msg stop" << std::endl;
        }

        if (isInjecting)
            patient->injectGlucose();
    }
    pthread_exit(NULL);
}

void *t_insuline(void *args) {
    Patient *patient = (Patient *)args;
    bool isInjecting = false;

    for(int i = 0; i < 10; i++) {
        Message msg;
        int byte_read = mq_receive(qr_insuline, (char *) &msg, MSG_SIZE, NULL);
        if (byte_read < 0) {
            std::cout << "Error receiving insuline msg" << std::endl;
        } else if (msg == START) {
            isInjecting = true;
            std::cout << "Insuline : Msg start" << std::endl;
        } else if (msg == STOP) {
            isInjecting = false;
            std::cout << "Insuline : Msg stop" << std::endl;
        }

        if (isInjecting)
            patient->injectInsuline();
    }
    pthread_exit(NULL);
}


int main(int argc, char **argv) {
    std::cout << "Hello world : " << sizeof(Message) << std::endl;

    Patient patient;
    pthread_mutex_init(&patient.m_glucose, NULL);
    pthread_mutex_init(&patient.m_insuline, NULL);

    mq_attr attr_glucose, attr_insuline;

    /* initialize the queue attributes */
    attr_glucose.mq_flags = 0;
    attr_glucose.mq_maxmsg = 50;
    attr_glucose.mq_msgsize = MSG_SIZE;

    attr_insuline.mq_flags = 0;
    attr_insuline.mq_maxmsg = 50;
    attr_insuline.mq_msgsize = MSG_SIZE;

    mq_unlink("q_glucose");
    qr_glucose = mq_open("q_glucose",
            O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr_glucose);
    if(qr_glucose == (mqd_t)0)
        std::cout << "ERROR1" << std::endl;

    qw_glucose= mq_open("q_glucose", O_WRONLY);
    if(qw_glucose == (mqd_t)0)
        std::cout << "ERROR2" << std::endl;

    mq_unlink("q_insuline");
    qr_insuline = mq_open("q_insuline",
            O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr_insuline);
    if(qr_insuline == (mqd_t)0)
        std::cout << "ERROR1" << std::endl;

    qw_insuline = mq_open("q_insuline", O_WRONLY);
    if(qw_insuline == (mqd_t)0)
        std::cout << "ERROR2" << std::endl;

    pthread_t th_glucose;
    pthread_create(&th_glucose, NULL, t_glucose, &patient);

    pthread_t th_insuline;
    pthread_create(&th_insuline, NULL, t_insuline, &patient);

    pthread_t th_patient;
    pthread_create(&th_patient, NULL, t_patient, &patient);

    pthread_join(th_glucose, NULL);
    pthread_join(th_patient, NULL);

    mq_close(qr_glucose);
    mq_close(qw_glucose);
    mq_unlink("q_glucose");

    mq_close(qr_insuline);
    mq_close(qw_insuline);
    mq_unlink("q_insuline");

    pthread_mutex_destroy(&patient.m_glucose);
    pthread_mutex_destroy(&patient.m_insuline);

    pthread_exit(NULL);
}
