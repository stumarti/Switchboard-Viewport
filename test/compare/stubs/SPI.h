#pragma once
#define HSPI 2
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings { SPISettings(int, int, int) {} };
struct SPIClass { SPIClass(int) {} void begin(int, int, int, int) {} };
