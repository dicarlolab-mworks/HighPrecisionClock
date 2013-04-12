//
//  HighPrecisionClock.cpp
//  HighPrecisionClock
//
//  Created by Christopher Stawarz on 4/12/13.
//  Copyright (c) 2013 The MWorks Project. All rights reserved.
//

#include "HighPrecisionClock.h"


BEGIN_NAMESPACE_MW


HighPrecisionClock::HighPrecisionClock() :
    systemBaseTime(mach_absolute_time())
{
    mach_timebase_info_data_t timebaseInfo;
    if (KERN_SUCCESS != mach_timebase_info(&timebaseInfo) ||
        timebaseInfo.numer != timebaseInfo.denom) {
        throw SimpleException(M_SCHEDULER_MESSAGE_DOMAIN, "HighPrecisionClock is not supported on this platform");
    }
}


MWTime HighPrecisionClock::getCurrentTimeNS() {
    return MWTime(mach_absolute_time() - systemBaseTime);
}


void HighPrecisionClock::sleepNS(MWTime time) {
    kern_return_t status = mach_wait_until(mach_absolute_time() + uint64_t(time));
    if (KERN_SUCCESS != status) {
        merror(M_SCHEDULER_MESSAGE_DOMAIN, "mach_wait_until failed: %s (%d)", mach_error_string(status), status);
    }
}


MWTime HighPrecisionClock::getSystemTimeNS() {
    return MWTime(mach_absolute_time());
}


MWTime HighPrecisionClock::getSystemBaseTimeNS() {
    return MWTime(systemBaseTime);
}


END_NAMESPACE_MW

























