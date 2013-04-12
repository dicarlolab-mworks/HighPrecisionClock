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


HighPrecisionClock::~HighPrecisionClock() {
    stopClock();
}


MWTime HighPrecisionClock::getSystemBaseTimeNS() {
    return MWTime(systemBaseTime);
}


MWTime HighPrecisionClock::getSystemTimeNS() {
    return MWTime(mach_absolute_time());
}


MWTime HighPrecisionClock::getCurrentTimeNS() {
    return MWTime(mach_absolute_time() - systemBaseTime);
}


void HighPrecisionClock::startClock() {
    if (!isRunning()) {
        try {
            runLoopThread = boost::thread(boost::bind(&HighPrecisionClock::runLoop,
                                                      component_shared_from_this<HighPrecisionClock>()));
        } catch (const boost::thread_resource_error &e) {
            merror(M_SCHEDULER_MESSAGE_DOMAIN, "Unable to start HighPrecisionClock: %s", e.what());
        }
    }
}


void HighPrecisionClock::stopClock() {
    if (isRunning()) {
        runLoopThread.interrupt();
        try {
            runLoopThread.join();
        } catch (const boost::system::system_error &e) {
            merror(M_SCHEDULER_MESSAGE_DOMAIN, "Unable to stop HighPrecisionClock: %s", e.what());
        }
    }
}


void HighPrecisionClock::sleepNS(MWTime time) {
    kern_return_t status = mach_wait_until(mach_absolute_time() + uint64_t(time));
    if (KERN_SUCCESS != status) {
        merror(M_SCHEDULER_MESSAGE_DOMAIN, "mach_wait_until failed: %s (%d)", mach_error_string(status), status);
    }
}


void HighPrecisionClock::runLoop() {
    if (!set_realtime(periodUS * nanosPerMicro,
                      computationUS * nanosPerMicro,
                      constraintUS * nanosPerMicro))
    {
        merror(M_SCHEDULER_MESSAGE_DOMAIN, "HighPrecisionClock failed to achieve real time scheduling");
    }
    
    while (true) {
        // Sleep for 500ms
        sleepMS(500);
        
        // Give another thread a chance to terminate this one
        boost::this_thread::interruption_point();
    }
}


END_NAMESPACE_MW

























