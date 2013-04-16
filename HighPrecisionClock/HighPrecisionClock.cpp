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
        kern_return_t status = semaphore_create(mach_task_self(), &newSem, SYNC_POLICY_FIFO, 0);
        if (KERN_SUCCESS != status) {
            merror(M_SCHEDULER_MESSAGE_DOMAIN, "semaphore_create failed: %s (%d)", mach_error_string(status), status);
            return;
        }
        threadSpecificSemaphore.reset(new semaphore_t(newSem));
        sem = threadSpecificSemaphore.get();
    }
    
    {
        lock_guard lock(waitsMutex);
        waits.push(WaitInfo(expirationTime, *sem));
    }
    
    kern_return_t status = semaphore_wait(*sem);
    if (KERN_SUCCESS != status) {
        merror(M_SCHEDULER_MESSAGE_DOMAIN, "semaphore_wait failed: %s (%d)", mach_error_string(status), status);
    }
}


void HighPrecisionClock::destroySemaphore(semaphore_t *sem) {
    if (sem) {
        kern_return_t status = semaphore_destroy(mach_task_self(), *sem);
        if (KERN_SUCCESS != status) {
            merror(M_SCHEDULER_MESSAGE_DOMAIN, "semaphore_destroy failed: %s (%d)", mach_error_string(status), status);
        }
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
                kern_return_t status = semaphore_signal(waits.top().getSemaphore());
                if (KERN_SUCCESS != status) {
                    merror(M_SCHEDULER_MESSAGE_DOMAIN, "semaphore_signal failed: %s (%d)", mach_error_string(status), status);
                }
                
                waits.pop();
            }
        }
        
        // Sleep until the next work cycle
        if (mach_absolute_time() < nextPeriodStart) {
            kern_return_t status = mach_wait_until(nextPeriodStart);
            if (KERN_SUCCESS != status) {
                merror(M_SCHEDULER_MESSAGE_DOMAIN, "mach_wait_until failed: %s (%d)", mach_error_string(status), status);
            }
        }
        
        // Give another thread a chance to terminate this one
        boost::this_thread::interruption_point();
    }
}


END_NAMESPACE_MW

























