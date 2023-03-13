/*
 * TimeUtil.h
 *
 *  Created on: Mar 13, 2023
 *      Author: edaniley
 */

#ifndef INCLUDE_TIMEUTIL_H_
#define INCLUDE_TIMEUTIL_H_
#ifdef unix
#include <x86intrin.h>
#else
#include <intrin.h>
#endif

namespace hw {

static inline  uint64_t rdtsc(){
    return __rdtsc();
}




}

#endif /* INCLUDE_TIMEUTIL_H_ */
