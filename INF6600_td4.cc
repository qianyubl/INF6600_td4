#include <cstdlib>
#include <iostream>

#include <pthread.h>
#include <mqueue.h>
#include <string>
#include <errno.h>

#define MSG_SIZE 4096

mqd_t qw_glucose;
mqd_t qr_glucose;
mqd_t qw_insuline;
mqd_t qr_insuline;
mqd_t qw_display;
mqd_t qr_display;

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

enum Message {STOP,
    START,
    NONE,
    HALT,
    GLYCEMIA_CRITICAL,
    GLYCEMIA_NORMAL
};

void *t_controller(void *args) {
    Patient *patient = (Patient *)args;

    for (int i = 0; i < 40; i++) {
        sleep(0.1);
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
                std::cout << "Error sending insuline msg" << std::endl;
        } else if (glycemia >= patient->glycemia_ref) {
            Message msg_glucose = STOP;
            Message msg_insuline = START;
            if(mq_send(qw_glucose, (const char *) &msg_glucose,
                        sizeof(msg_glucose), 0) < 0)
                std::cout << "Error sending glucose msg" << std::endl;
            if(mq_send(qw_insuline, (const char *) &msg_insuline,
                        sizeof(msg_insuline), 0) < 0)
                std::cout << "Error sending insuline  msg" << std::endl;
        }
    }

    Message msg = HALT;
    if(mq_send(qw_glucose, (const char *) &msg,
                sizeof(msg), 0) < 0)
        std::cout << "Error sending glucose msg halt" << std::endl;
    if(mq_send(qw_insuline, (const char *) &msg,
                sizeof(msg), 0) < 0)
        std::cout << "Error sending glucose msg halt" << std::endl;

    pthread_exit(NULL);
}

void *t_glucose(void *args) {
    Patient *patient = (Patient *)args;
    bool isInjecting = false;

    while (true) {
        sleep(0.1);
        Message msg = NONE;
        mq_attr attr, old_attr;
        mq_getattr(qr_glucose, &attr);
        if (attr.mq_curmsgs != 0) {
            // First set the queue to not block any calls
            attr.mq_flags = O_NONBLOCK;
            mq_setattr(qr_glucose, &attr, &old_attr);
            // Now consume all of the messages. Only the last one is usefull
            while (mq_receive(qr_glucose, (char *) &msg, MSG_SIZE, NULL) != -1)
            {}
            if (errno == EAGAIN)
                std::cout << "No more messages in glucose queue" << std::endl;
            else
                std::cout << "Error receiving glucose msg" << std::endl;

            // Now restore the attributes
            mq_setattr(qr_glucose, &old_attr, NULL);
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
    Patient *patient = (Patient *)args;
    bool isInjecting = true;

    while (true) {
        sleep(0.1);
        Message msg = NONE;

        mq_attr attr, old_attr;
        mq_getattr (qr_insuline, &attr);
        if (attr.mq_curmsgs != 0) {
            // There are some messages on this queue....eat em
            // First set the queue to not block any calls
            attr.mq_flags = O_NONBLOCK;
            mq_setattr(qr_insuline, &attr, &old_attr);
            // Now eat all of the messages
            while (mq_receive(qr_insuline, (char *) &msg, MSG_SIZE, NULL) != -1)
            {}
            if (errno == EAGAIN)
                std::cout << "No more messages in insuline queue" << std::endl;
            else
                std::cout << "Error receiving insuline msg" << std::endl;

            // Now restore the attributes
            mq_setattr (qr_insuline, &old_attr, NULL);
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

    while (true) {
        Message msg = NONE;
        if (mq_receive(qr_display, (char *) &msg, MSG_SIZE, NULL) == -1) {
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

        if (msg == STOP) {
            pthread_exit(NULL);
        }
    }
}

int main(int argc, char **argv) {

    Patient patient;

    /* initialize the queue attributes */
    mq_attr attr_glucose, attr_insuline, attr_display;
    attr_glucose.mq_flags = 0;
    attr_glucose.mq_maxmsg = 50;
    attr_glucose.mq_msgsize = MSG_SIZE;

    attr_insuline.mq_flags = 0;
    attr_insuline.mq_maxmsg = 50;
    attr_insuline.mq_msgsize = MSG_SIZE;

    attr_display.mq_flags = 0;
    attr_display.mq_maxmsg = 50;
    attr_display.mq_msgsize = MSG_SIZE;

    mq_unlink("q_glucose");
    qr_glucose = mq_open("q_glucose",
            O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr_glucose);
    if(qr_glucose == (mqd_t)0)
        std::cout << "Error creating `qr_glucose`" << std::endl;

    qw_glucose= mq_open("q_glucose", O_WRONLY);
    if(qw_glucose == (mqd_t)0)
        std::cout << "Error creating `qw_glucose`" << std::endl;

    mq_unlink("q_insuline");
    qr_insuline = mq_open("q_insuline",
            O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr_insuline);
    if(qr_insuline == (mqd_t)0)
        std::cout << "Error creating `qr_insuline`" << std::endl;

    qw_insuline = mq_open("q_insuline", O_WRONLY);
    if(qw_insuline == (mqd_t)0)
        std::cout << "Error creating `qw_insuline`"<< std::endl;

    mq_unlink("q_display");
    qr_display = mq_open("q_display",
            O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr_display);
    if(qr_display == (mqd_t)0)
        std::cout << "Error creating `qr_display`" << std::endl;

    qw_insuline = mq_open("q_insuline", O_WRONLY);
    if(qw_insuline == (mqd_t)0)
        std::cout << "Error creating `qw_insuline`"<< std::endl;

    pthread_t th_controller;
    pthread_create(&th_controller, NULL, t_controller, &patient);

    pthread_t th_glucose;
    pthread_create(&th_glucose, NULL, t_glucose, &patient);

    pthread_t th_insuline;
    pthread_create(&th_insuline, NULL, t_insuline, &patient);

    pthread_t th_display;
    pthread_create(&th_display, NULL, t_display, NULL);

    pthread_join(th_controller, NULL);
    pthread_join(th_glucose, NULL);
    pthread_join(th_insuline, NULL);
    pthread_join(th_display, NULL);

    mq_close(qr_glucose);
    mq_close(qw_glucose);
    mq_unlink("q_glucose");

    mq_close(qr_insuline);
    mq_close(qw_insuline);
    mq_unlink("q_insuline");

    pthread_exit(NULL);
}
