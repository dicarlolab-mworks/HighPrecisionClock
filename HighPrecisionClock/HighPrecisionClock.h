//
//  HighPrecisionClock.h
//  HighPrecisionClock
//
//  Created by Christopher Stawarz on 4/12/13.
//  Copyright (c) 2013 The MWorks Project. All rights reserved.
//

#ifndef __HighPrecisionClock__HighPrecisionClock__
#define __HighPrecisionClock__HighPrecisionClock__


BEGIN_NAMESPACE_MW


class HighPrecisionClock : public Clock, boost::noncopyable {
    
public:
    HighPrecisionClock();
    
    MWTime getCurrentTimeNS() MW_OVERRIDE;
    
    void sleepNS(MWTime time) MW_OVERRIDE;
    
    MWTime getSystemTimeNS() MW_OVERRIDE;
    MWTime getSystemBaseTimeNS() MW_OVERRIDE;
    
private:
    const uint64_t systemBaseTime;
    
};


END_NAMESPACE_MW


#endif // !defined(__HighPrecisionClock__HighPrecisionClock__)
