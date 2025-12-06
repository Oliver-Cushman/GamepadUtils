#ifndef JSEVENT_H
#define JSEVENT_H

#include <sys/types.h>
#include <cstdint>

struct JSEvent
{
    uint time;      /* event timestamp in milliseconds */
    short value;    /* value */
    uint8_t type;   /* event type (1: btn, 2: axis) */
    uint8_t number; /* axis/button number */
};

#endif // JSEVENT_H