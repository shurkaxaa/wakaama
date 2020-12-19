#pragma once

#ifndef _LOG_H_
#define _LOG_H

#ifdef LWM2M_WITH_LOGS
#define ZF_LOG_LEVEL ZF_LOG_VERBOSE
#else
#define ZF_LOG_LEVEL ZF_LOG_ERROR
#endif

#include "zf_log.h"

#endif
