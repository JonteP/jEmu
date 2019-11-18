#ifndef NES_FDS_H_
#define NES_FDS_H_
#include <stdint.h>

void fds_load_disk(char *), run_fds(uint16_t);
void write_fds_register(uint16_t, uint8_t);
void init_fds(void);
uint8_t read_fds_register(uint16_t);

extern uint8_t *fdsBiosRom, fdsRam[0x8000], currentDiskSide, diskFlag;

#endif
