#ifndef INCLUDES_H
#define INCLUDES_H

#ifdef WINDOWS_TEST
  #include "mock_avr_io.h"
  #include "delay.h"         // picks up lib/Mocks/util/delay.h
#else
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
