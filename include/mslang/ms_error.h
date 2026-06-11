#pragma once

typedef enum MsErrCode {
  MS_OK           = 0,
  MS_ERR_OOM      = 1,
  MS_ERR_IO       = 2,
  MS_ERR_SYNTAX   = 3,
  MS_ERR_RUNTIME  = 4,
  MS_ERR_IMPORT   = 5,
  MS_ERR_INTERNAL = 6,
} MsErrCode;

#ifndef NDEBUG
void msInternalPanic(const char* file, int line, const char* expr);
#endif

#ifdef NDEBUG
#  define MS_ASSERT(cond) ((void)0)
#else
#  define MS_ASSERT(cond) \
  ((cond) ? (void)0 : (msInternalPanic(__FILE__, __LINE__, #cond), (void)0))
#endif
