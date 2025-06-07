#ifndef INCLUDES_H
#define INCLUDES_H

#ifdef WINDOWS_TEST
  // On native builds, use the mock and stub implementations
  #include "mock_avr_io.h"
  #include "util/delay.h"      // stub from lib/Mocks/util/delay.h
#else
  // On AVR, include real hardware headers
  #include <avr/io.h>
  #include <avr/interrupt.h>
  #include <util/delay.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#endif // INCLUDES_H