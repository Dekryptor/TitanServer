#include "stdafx.h"
#include "RC4.h"

void rc4Init(RC4_STATE *rc4State, unsigned char *rc4Key, unsigned int rc4KeyLength) {
	unsigned char t; // temporary storage
	unsigned int j = 0;
	unsigned int k = 0;

	// setup our state values
	rc4State->x = 0;
	rc4State->y = 0;

	// setup our sbox values
	for (unsigned int i = 0; i < 256; i++) {
		rc4State->sbox[i] = i;
	}

	for (unsigned int i = 0; i < 256; i++) {
		k = (rc4Key[j] + rc4State->sbox[i] + k) % 256;

		// swap k and i
		t = rc4State->sbox[i];
		rc4State->sbox[i] = rc4State->sbox[k];
		rc4State->sbox[k] = t;         

        j = (j + 1) % rc4KeyLength;  
	}
}

void rc4Process(RC4_STATE *rc4State, unsigned char *data, unsigned int dataLength) {
	unsigned char t; // temporary storage
	unsigned char n; // our index to the byte to XOR with
	unsigned char x = rc4State->x;
	unsigned char y = rc4State->y;

	for (int i = 0; i < dataLength; i++) {
		x = (x + 1) % 256;                      
		y = (rc4State->sbox[x] + y) % 256;   

		// swap x and y
		t = rc4State->sbox[x];
		rc4State->sbox[x] = rc4State->sbox[y];
		rc4State->sbox[y] = t;                       
               
		n = (rc4State->sbox[x] + rc4State->sbox[y]) % 256;                 
               
		data[i] ^= rc4State->sbox[n];
	}

	rc4State->x = x;
	rc4State->y = y;
}

void rc4Quick(unsigned char *rc4Key, unsigned int rc4KeyLength, unsigned char *data, unsigned int dataLength) {
	RC4_STATE rc4State;
	rc4Init(&rc4State, rc4Key, rc4KeyLength);
	rc4Process(&rc4State, data, dataLength);
}
