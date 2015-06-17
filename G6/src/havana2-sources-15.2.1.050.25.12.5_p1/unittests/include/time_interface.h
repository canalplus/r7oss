#ifndef __TIME_ITF_H
#define __TIME_ITF_H

class TimeInterface {
  public:
    virtual unsigned long long GetTimeInMicroSeconds() = 0;
};


void useTimeInterface(TimeInterface *timeInterface);

#endif
