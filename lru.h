#pragma once

#include "mem_access.h"
#include "addr.h"
#include "cache.h"


#define LRU_age_increase(TYPE, WAYS, WAY_INDEX, LINE_INDEX) \
    { \
    foreach_way(var, WAYS){  \
        if(var == WAY_INDEX){   \
            cache_age(TYPE, WAYS, LINE_INDEX, var) = 0; \
        } \
        if(cache_age(TYPE, WAYS, LINE_INDEX, var) < (WAYS - 1)) { \
            cache_age(TYPE, WAYS, LINE_INDEX, var) += 1; \
        }   \
    }}


#define LRU_age_update(TYPE, WAYS, WAY_INDEX, LINE_INDEX) \
    { \
        foreach_way(var, WAYS){  \
            if (cache_age(TYPE, WAYS, LINE_INDEX, var) < cache_age(TYPE, WAYS, LINE_INDEX, WAY_INDEX)) { \
                cache_age(TYPE, WAYS, LINE_INDEX, var) += 1; \
            } \
            cache_age(TYPE, WAYS, LINE_INDEX, WAY_INDEX) = 0; \
        } \
    }
