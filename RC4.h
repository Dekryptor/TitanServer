// RC4 library
// mojobojo 2014
#ifndef __RC4_H_
#define __RC4_H_

typedef struct {
	unsigned char sbox[256];
	unsigned char x;
	unsigned char y;
} RC4_STATE;

void rc4Init(RC4_STATE *rc4State, unsigned char *rc4Key, unsigned int rc4KeyLength);
void rc4Process(RC4_STATE *rc4State, unsigned char *data, unsigned int dataLength);
void rc4Quick(unsigned char *rc4Key, unsigned int rc4KeyLength, unsigned char *data, unsigned int dataLength);

#endif
