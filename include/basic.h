#pragma once

#include <stdexcept>
#include <sstream>
#include <memory>

#include "byteset.h"
#include "lightgrep_c_api.h"
#include "rangeset.h"

typedef RangeSet<uint32,0x110000> UnicodeSet;

typedef unsigned char byte;

typedef unsigned short uint16;

//typedef unsigned int uint32;
typedef int int32;

//typedef unsigned long long uint64;
typedef long long int64;

#define THROW_WITH_OUTPUT(exceptType, expression) \
  std::stringstream buf; \
  buf << __FILE__ << ":" << __LINE__ << ": " << expression; \
  throw exceptType(buf.str())

#define THROW_RUNTIME_ERROR_WITH_OUTPUT(expression) THROW_WITH_OUTPUT(std::runtime_error, expression)

#define THROW_RUNTIME_ERROR_WITH_CLEAN_OUTPUT(expression) \
  std::stringstream buf; \
  buf << expression; \
  throw std::runtime_error(buf.str())
