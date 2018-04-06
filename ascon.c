#include <stdio.h>
#include "api.h"
#include "crypto_aead.h"

typedef unsigned char u8;
typedef unsigned long long u64;
typedef long long i64;

//#define PRINTSTATE
//#define PRINTWORDS

void printstate(char* text, u8* S) {
#ifdef PRINTSTATE
  int i;
  printf("%s\n", text);
  for (i = 0; i < 40; ++i)
    printf("%02x", S[i]);
  printf("\n");
#endif
}

void printwords(char* text, u64 x0, u64 x1, u64 x2, u64 x3, u64 x4) {
#ifdef PRINTWORDS
  int i;
  printf("%s\n", text);
  printf("  x0=%016llx\n", x0);
  printf("  x1=%016llx\n", x1);
  printf("  x2=%016llx\n", x2);
  printf("  x3=%016llx\n", x3);
  printf("  x4=%016llx\n", x4);
#endif
}

void load64(u64* x, u8* S) {
  int i;
  *x = 0;
  for (i = 0; i < 8; ++i)
    *x |= ((u64) S[i]) << (56 - i * 8);
}

void store64(u8* S, u64 x) {
  int i;
  for (i = 0; i < 8; ++i)
    S[i] = (u8) (x >> (56 - i * 8));
}

void load64_grouped(u8* S, u64 x[5]){
        load64(x, S + 0);
        load64(x + 1, S + 8);
        load64(x + 2, S + 16);
        load64(x + 3, S + 24);
        load64(x + 4, S + 32);
}

void store64_grouped(u8* S, u64 x[5]){
        store64(S + 0, x[0]);
        store64(S + 8, x[1]);
        store64(S + 16, x[2]);
        store64(S + 24, x[3]);
        store64(S + 32, x[4]);
}

extern void permutation(u8* S, int start, int rounds);

permutation_prep(u8* S, int start, int rounds) {
        int i;
		u64 x0, x1, x2, x3, x4;
		load64(&x0, S + 0);
		load64(&x1, S + 8);
		load64(&x2, S + 16);
		load64(&x3, S + 24);
		load64(&x4, S + 32);
		/**for (i = 0; i < 8; i++){
                  *x |= ((u64) S[i]) << (56 - i * 8);
                  *(x+1) |= ((u64) S[i+8]) << (56 - i * 8);
                  *(x+2) |= ((u64) S[i+16]) << (56 - i * 8);
                  *(x+3) |= ((u64) S[i+24]) << (56 - i * 8);
                  *(x+4) |= ((u64) S[i+32]) << (56 - i * 8);
        }*/
   //   load64_grouped(S, x);
        permutation(S, start, rounds);
//      store64_grouped(S, x);
        /**for (i = 0; i < 8; i++){
                S[i] = (u8) (x[0] >> (56 - i * 8));
                S[i+8] = (u8) (x[1] >> (56 - i * 8));
                S[i+16] = (u8) (x[2] >> (56 - i * 8));
                S[i+24] = (u8) (x[3] >> (56 - i * 8));
                S[i+32] = (u8) (x[4] >> (56 - i * 8));
        }*/
		store64(S + 0, x0);
		store64(S + 8, x1);
		store64(S + 16, x2);
		store64(S + 24, x3);
		store64(S + 32, x4);
}


int crypto_aead_encrypt(
    unsigned char *c, unsigned long long *clen,
    const unsigned char *m, unsigned long long mlen,
    const unsigned char *ad, unsigned long long adlen,
    const unsigned char *nsec,
    const unsigned char *npub,
    const unsigned char *k) {

  int klen = CRYPTO_KEYBYTES;
  int size = 320 / 8;
  //int nlen = CRYPTO_NPUBBYTES;
  int rate = 64 / 8;
  int capacity = size - rate;
  int a = 12;
  int b = 6;
  i64 s = adlen / rate + 1;
  i64 t = mlen / rate + 1;
  i64 l = mlen % rate;

  u8 S[size];
  u8 A[s * rate];
  u8 M[t * rate];
  i64 i, j;

  // pad associated data
  for (i = 0; i < adlen; ++i)
    A[i] = ad[i];
  A[adlen] = 0x80;
  for (i = adlen + 1; i < s * rate; ++i)
    A[i] = 0;
  // pad plaintext
  for (i = 0; i < mlen; ++i)
    M[i] = m[i];
  M[mlen] = 0x80;
  for (i = mlen + 1; i < t * rate; ++i)
    M[i] = 0;

  // initialization
  S[0] = klen * 8;
  S[1] = rate * 8;
  S[2] = a;
  S[3] = b;
  for (i = 4; i < size - 2 * klen; ++i)
    S[i] = 0;
  for (i = 0; i < klen; ++i)
    S[size - 2 * klen + i] = k[i];
  for (i = 0; i < klen; ++i)
    S[size - klen + i] = npub[i];
  printstate("initial value:", S);
  permutation(S, 12 - a, a);
  for (i = 0; i < klen; ++i)
    S[rate + klen + i] ^= k[i];
  printstate("initialization:", S);

  // process associated data
  if (adlen != 0) {
    for (i = 0; i < s; ++i) {
      for (j = 0; j < rate; ++j)
        S[j] ^= A[i * rate + j];
      permutation(S, 12 - b, b);
    }
  }
  S[size - 1] ^= 1;
  printstate("process associated data:", S);

  // process plaintext
  for (i = 0; i < t - 1; ++i) {
    for (j = 0; j < rate; ++j) {
      S[j] ^= M[i * rate + j];
      c[i * rate + j] = S[j];
    }
    permutation(S, 12 - b, b);
  }
  for (j = 0; j < rate; ++j)
    S[j] ^= M[(t - 1) * rate + j];
  for (j = 0; j < l; ++j)
    c[(t - 1) * rate + j] = S[j];
  printstate("process plaintext:", S);

  // finalization
  for (i = 0; i < klen; ++i)
    S[rate + i] ^= k[i];
  permutation(S, 12 - a, a);
  for (i = 0; i < klen; ++i)
    S[rate + klen + i] ^= k[i];
  printstate("finalization:", S);

  // return tag
  for (i = 0; i < klen; ++i)
    c[mlen + i] = S[rate + klen + i];
  *clen = mlen + klen;

  return 0;
}

int crypto_aead_decrypt(
    unsigned char *m, unsigned long long *mlen,
    unsigned char *nsec,
    const unsigned char *c, unsigned long long clen,
    const unsigned char *ad, unsigned long long adlen,
    const unsigned char *npub,
    const unsigned char *k) {

  *mlen = 0;
  if (clen < CRYPTO_KEYBYTES)
    return -1;

  int klen = CRYPTO_KEYBYTES;
  //int nlen = CRYPTO_NPUBBYTES;
  int size = 320 / 8;
  int rate = 64 / 8;
  int capacity = size - rate;
  int a = 12;
  int b = 6;
  i64 s = adlen / rate + 1;
  i64 t = (clen - klen) / rate + 1;
  i64 l = (clen - klen) % rate;

  u8 S[size];
  u8 A[s * rate];
  u8 M[t * rate];
  i64 i, j;

  // pad associated data
  for (i = 0; i < adlen; ++i)
    A[i] = ad[i];
  A[adlen] = 0x80;
  for (i = adlen + 1; i < s * rate; ++i)
    A[i] = 0;

  // initialization
  S[0] = klen * 8;
  S[1] = rate * 8;
  S[2] = a;
  S[3] = b;
  for (i = 4; i < size - 2 * klen; ++i)
    S[i] = 0;
  for (i = 0; i < klen; ++i)
    S[size - 2 * klen + i] = k[i];
  for (i = 0; i < klen; ++i)
    S[size - klen + i] = npub[i];
  printstate("initial value:", S);
  permutation(S, 12 - a, a);
  for (i = 0; i < klen; ++i)
    S[rate + klen + i] ^= k[i];
  printstate("initialization:", S);

  // process associated data
  if (adlen) {
    for (i = 0; i < s; ++i) {
      for (j = 0; j < rate; ++j)
        S[j] ^= A[i * rate + j];
      permutation(S, 12 - b, b);
    }
  }
  S[size - 1] ^= 1;
  printstate("process associated data:", S);

  // process plaintext
  for (i = 0; i < t - 1; ++i) {
    for (j = 0; j < rate; ++j) {
      M[i * rate + j] = S[j] ^ c[i * rate + j];
      S[j] = c[i * rate + j];
    }
    permutation(S, 12 - b, b);
  }
  for (j = 0; j < l; ++j)
    M[(t - 1) * rate + j] = S[j] ^ c[(t - 1) * rate + j];
  for (j = 0; j < l; ++j)
    S[j] = c[(t - 1) * rate + j];
  S[l] ^= 0x80;
  printstate("process plaintext:", S);

  // finalization
  for (i = 0; i < klen; ++i)
    S[rate + i] ^= k[i];
  permutation(S, 12 - a, a);
  for (i = 0; i < klen; ++i)
    S[rate + klen + i] ^= k[i];
  printstate("finalization:", S);

  // return -1 if verification fails
  for (i = 0; i < klen; ++i)
    if (c[clen - klen + i] != S[rate + klen + i])
      return -1;

  // return plaintext
  *mlen = clen - klen;
  for (i = 0; i < *mlen; ++i)
    m[i] = M[i];

  return 0;
}