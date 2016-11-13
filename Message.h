
#ifndef MESSAGE_H
#define MESSAGE_H

#define MSG_SIZE 4096

struct MQHandler {
    // declaration of messages queues descriptors
    mqd_t qw_glucose;
    mqd_t qr_glucose;
    mqd_t qw_insuline;
    mqd_t qr_insuline;
    mqd_t qw_display;
    mqd_t qr_display;
    // declaration of a condvar
    pthread_cond_t cv_syringe;

    MQHandler() {
        // initialize the queue attributes
        mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 50;
        attr.mq_msgsize = MSG_SIZE;

	// open the queue q_glucose in read only mode qr_glucose and in write mode qw_glucose 
        mq_unlink("q_glucose");
        qr_glucose = mq_open("q_glucose",
                O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr);
        if(qr_glucose == (mqd_t)0)
            std::cout << "Error creating `qr_glucose`" << std::endl;

        qw_glucose= mq_open("q_glucose", O_WRONLY);
        if(qw_glucose == (mqd_t)0)
            std::cout << "Error creating `qw_glucose`" << std::endl;

	// open the queue q_insuline in read mode qr_insuline and in write mode qw_insuline 
        mq_unlink("q_insuline");
        qr_insuline = mq_open("q_insuline",
                O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr);
        if(qr_insuline == (mqd_t)0)
            std::cout << "Error creating `qr_insuline`" << std::endl;

        qw_insuline = mq_open("q_insuline", O_WRONLY);
        if(qw_insuline == (mqd_t)0)
            std::cout << "Error creating `qw_insuline`"<< std::endl;

	  // open the queue q_display in read mode qr_display	  
  	  // and in write mode qw_display 
        mq_unlink("q_display");
        qr_display = mq_open("q_display",
                O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR, &attr);
        if(qr_display == (mqd_t)0)
            std::cout << "Error creating `qr_display`" << std::endl;

        qw_display = mq_open("q_display", O_WRONLY);
        if(qw_display == (mqd_t)0)
            std::cout << "Error creating `qw_display`"<< std::endl;

    pthread_cond_init(&cv_syringe, NULL);
    }

    ~MQHandler() {
        mq_close(qr_glucose);
        mq_close(qw_glucose);
        mq_unlink("q_glucose");

        mq_close(qr_insuline);
        mq_close(qw_insuline);
        mq_unlink("q_insuline");

        mq_close(qr_display);
        mq_close(qw_display);
        mq_unlink("q_display");
    }
};

// list all messages used in enum	  
enum Message {
    STOP,
    START,
    NONE,
    HALT,
    GLYCEMIA_CRITICAL,
    GLYCEMIA_NORMAL,
    GLUCOSE_START,
    GLUCOSE_STOP,
    INSULINE_START,
    INSULINE_STOP,
    ANTIBIO_INJECT,
    ANTICOAG_INJECT,
    SYRINGE_1_LOW,
    SYRINGE_2_LOW,
    SYRINGE_1_CRITICAL,
    SYRINGE_2_CRITICAL,
    SWITCH,
    RESET
};

#endif
