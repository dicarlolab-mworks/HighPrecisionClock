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
    systemBaseTime(mach_absolute_time()),
    threadSpecificSemaphore(&destroySemaphore)
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
            throw SimpleException(M_SCHEDULER_MESSAGE_DOMAIN, "Unable to start HighPrecisionClock", e.what());
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
    const uint64_t expirationTime = mach_absolute_time() + uint64_t(time);
    
    semaphore_t *sem = threadSpecificSemaphore.get();
    if (!sem) {
        semaphore_t newSem;
        if (logMachError("semaphore_create", semaphore_create(mach_task_self(), &newSem, SYNC_POLICY_FIFO, 0))) {
            return;
        }
        threadSpecificSemaphore.reset(new semaphore_t(newSem));
        sem = threadSpecificSemaphore.get();
    }
    
    {
        lock_guard lock(waitsMutex);
        waits.push(WaitInfo(expirationTime, *sem));
    }
    
    logMachError("semaphore_wait", semaphore_wait(*sem));
}


void HighPrecisionClock::destroySemaphore(semaphore_t *sem) {
    if (sem) {
        logMachError("semaphore_destroy", semaphore_destroy(mach_task_self(), *sem));
        delete sem;
    }
}


void HighPrecisionClock::runLoop() {
    const uint64_t period = periodUS * nanosPerMicro;
    
    if (!set_realtime(period, computationUS * nanosPerMicro, period)) {
        merror(M_SCHEDULER_MESSAGE_DOMAIN, "HighPrecisionClock failed to achieve real time scheduling");
    }
    
    while (true) {
        const uint64_t nextPeriodStart = mach_absolute_time() + period;
        
        {
            lock_guard lock(waitsMutex);
            while (!waits.empty() && waits.top().getExpirationTime() < nextPeriodStart) {
                logMachError("semaphore_signal", semaphore_signal(waits.top().getSemaphore()));
                waits.pop();
            }
        }
        
        // Give another thread a chance to terminate this one
        boost::this_thread::interruption_point();
        
        // Sleep until the next work cycle
        if (mach_absolute_time() < nextPeriodStart) {
            logMachError("mach_wait_until", mach_wait_until(nextPeriodStart));
        }
    }
}


END_NAMESPACE_MW

























