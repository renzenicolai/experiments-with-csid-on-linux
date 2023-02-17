#pragma once

#include <stdint.h>

#define MAX_DATA_LEN 65536

#define SIDMODEL_8580 8580
#define SIDMODEL_6581 6581

//#define DEFAULT_SAMPLERATE 44100
#define DEFAULT_SIDMODEL SIDMODEL_6581

extern void libcsid_init(int samplerate, int sidmodel);

extern int libcsid_load(unsigned char *buffer, int bufferlen, int subtune);

extern const char *libcsid_getauthor();
extern const char *libcsid_getinfo();
extern const char *libcsid_gettitle();

extern void libcsid_render(unsigned short *output, int numsamples);

extern void c64set(uint32_t addr, uint8_t value);
extern void sidset(uint8_t addr, uint8_t value);
extern uint8_t c64get(uint32_t addr);
extern uint8_t sidget(uint8_t addr);
extern void step(uint8_t *stream);
extern void steps(uint8_t *stream, int len);
