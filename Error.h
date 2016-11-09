#ifndef ERROR_H
#define ERROR_H

#define CHECK(c, msg)                       \
    do {                                    \
        if(!(c))                              \
            std::cerr << msg << std::endl;   \
    } while (false)                         \

#endif
