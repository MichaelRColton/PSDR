#ifndef _FREQUENCY_CONTROL_H
#define _FREQUENCY_CONTROL_H

#include <stdbool.h>

typedef enum {DDS, SI5338} synthType;



bool initFrequencySynth();
bool setFrequency(unsigned long long frequency);

#endif // _FREQUENCY_CONTROL_H
