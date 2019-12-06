#ifndef WIREENCODE_H
#define WIREENCODE_H

#include <stdint.h>
int encode_bin(uint8_t *bin, uint16_t binlen, uint8_t* buf, uint16_t buflen);
int decode_bin(uint8_t *bin, uint16_t binlen, uint8_t* buf, uint16_t buflen);

#endif
