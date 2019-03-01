#ifndef __NSRPART2H__
#define __NSRPART2H__

#include "nsr_part1.h"

/* [2/9.1] [3/9.1] Volume Structure Descriptor */
struct VolStructDesc {
    uint8_t structType;
    uint8_t stdIdentifier[5];
    uint8_t structVersion;
    uint8_t reserved;
    uint8_t structData[2040];
};

#define VRS_ISO9660        "CD001"
#define VRS_ISO13346_BEGIN "BEA01"
#define VRS_ISO13346_NSR   "NSR02"
#define VRS_ISO13346_END   "TEA01"



#endif
