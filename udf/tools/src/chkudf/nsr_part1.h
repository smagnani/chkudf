// SPDX-License-Identifier: MIT
// © Copyright 1996 Hewlett-Packard Development Company, L.P

#ifndef __NSRPART1H__
#define __NSRPART1H__

typedef uint8_t dstring;

/*
 * ----------- Definitions for basic structures ------------------
 */

/* [1/7.2.1] -----------------------------------------------------*/
struct charspec {
    uint8_t uCharSetType;
    uint8_t aCharSetInfo[63];
};
/* uCharSetType is one of: */
#define NSR_CS0  0
#define NSR_CS1  1
#define NSR_CS2  2
#define NSR_CS3  3
#define NSR_CS4  4
#define NSR_CS5  5
#define NSR_CS6  6
#define NSR_CS7  7
#define NSR_CS8  8

/* [1/7.3] -------------------------------------------------------*/
struct timestamp {
    int16_t uTypeAndTimeZone;
    int16_t  iYear;
    uint8_t uMonth;
    uint8_t uDay;
    uint8_t uHour;
    uint8_t uMinute;
    uint8_t uSecond;
    uint8_t uCentiseconds;
    uint8_t uHundredMicroseconds;
    uint8_t uMicroseconds;
};
/* CUT = Coordinated Universal Time; LOCAL = local time; BA = By agreement */
#define TIMETYPE_CUT   0
#define TIMETYPE_LOCAL 1
#define TIMETYPE_BA    2

/* Note: using a 16 bit bitfield is not possible with some compilers */
#define TPMask 0xf000
#define TPShift 12

#define TZMask 0x0fff
#define TZSignBit 0x0800
#define TZSignExt 0xf000

#define GetTSTP(ttz) (ttz>>TPShift)
#define GetTSTZ(ttz) ((int16_t)((ttz&TZSignBit)?(ttz|TZSignExt):(ttz&TZMask)))
#define SetTSTP(ttz,tp) ((tp<<TPShift) | (ttz&TZMask))
#define SetTSTZ(ttz,tz) ((ttz&TPMask) | (tz&TZMask))


/* [1/7.4] ISO-definition of regid -------------------------------*/
struct regid {
    uint8_t uFlags;
    uint8_t aRegisteredID[23];
    uint8_t aIDSuffix[8];
};

#define REGID_FLAGS_DIRTY     0x01
#define REGID_FLAGS_PROTECTED 0x02

/*
 * UDF1.01 / 2.1.4.2
 * This always contains "*OSTA UDF Compliant" in aID
 * and DOMAIN identifier suffix in the suffix area.
 * uUDFRevision = 0x0100
 * uDomainFlags usually = 0.
 */
struct domainEntityId {
    uint8_t  uFlags;
    uint8_t  aID[23];
    uint16_t uUDFRevision;
    uint8_t  uDomainFlags;
    uint8_t  aReserved[5];
};

/*
 * UDF1.01 / 2.1.4.2
 * This is used with implementation use identifiers that are defined in
 * UDF.  uUDF revision is as above.
 * uOSClass is defined in constants in nsr.h.
 * uOSIdentifier is defined in constants in nsr.h.
 */
struct udfEntityId {
    uint8_t  uFlags;
    uint8_t  aID[23];
    uint16_t uUDFRevision;
    uint8_t  uOSClass;
    uint8_t  uOSIdentifier;
    uint8_t  aReserved[4];
};

/* 
 * UDF1.01 / 2.1.4.2
 * This type of identifier contains the name of the implementation that
 * generated the structure
 */

struct implEntityId {
    uint8_t uFlags        ;
    uint8_t aID[23]       ;
    uint8_t uOSClass      ;
    uint8_t uOSIdentifier ;
    uint8_t uImplUse[6]   ;
};

/* 
 * UDF1.01 / 2.1.4.2
 * This type of identifier contains the name of the implementation that
 * generated the structure (HP specific)
 */

struct HPimplEntityId {
    uint8_t  uFlags        ;
    uint8_t  aID[23]       ;
    uint8_t  uOSClass      ;
    uint8_t  uOSIdentifier ;
    uint8_t  uDescVersion  ;
    uint8_t  uLibrVersion  ;
    uint16_t uImplRegNo   ;
    uint8_t  uImplVersion  ;
    uint8_t  uImplRevision ;
};


#endif
