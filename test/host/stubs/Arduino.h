// Host stand-in for the bits of Arduino the firmware's drawing code and the
// kitchen panel's asset headers use.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "pgmspace.h"
#include "WString.h"
#include "Print.h"

inline unsigned long millis() { return 0; }
