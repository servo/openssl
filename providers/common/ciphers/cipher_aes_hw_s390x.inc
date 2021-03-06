/*
 * Copyright 2001-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * IBM S390X support for AES modes ecb, cbc, ofb, cfb, ctr.
 * This file is included by cipher_aes_hw.c
 */

#include "s390x_arch.h"

#define s390x_aes_cbc_initkey    cipher_hw_aes_initkey
#define s390x_aes_cfb1_initkey   cipher_hw_aes_initkey
#define s390x_aes_ctr_initkey    cipher_hw_aes_initkey
#define s390x_aes_cbc_cipher_hw  cipher_hw_generic_cbc
#define s390x_aes_cfb1_cipher_hw cipher_hw_generic_cfb1
#define s390x_aes_ctr_cipher_hw  cipher_hw_generic_ctr

static int s390x_aes_ecb_initkey(PROV_CIPHER_CTX *dat,
                                 const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    if (!dat->enc)
        adat->plat.s390x.fc |= S390X_DECRYPT;

    memcpy(adat->plat.s390x.param.km.k, key, keylen);
    return 1;
}

static int s390x_aes_ecb_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
                                   const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    s390x_km(in, len, out, adat->plat.s390x.fc, &adat->plat.s390x.param.km);
    return 1;
}

static int s390x_aes_ofb_initkey(PROV_CIPHER_CTX *dat,
                                 const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    memcpy(adat->plat.s390x.param.kmo_kmf.cv, dat->iv, dat->blocksize);
    memcpy(adat->plat.s390x.param.kmo_kmf.k, key, keylen);
    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    adat->plat.s390x.res = 0;
    return 1;
}

static int s390x_aes_ofb_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
                                   const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    int n = adat->plat.s390x.res;
    int rem;

    while (n && len) {
        *out = *in ^ adat->plat.s390x.param.kmo_kmf.cv[n];
        n = (n + 1) & 0xf;
        --len;
        ++in;
        ++out;
    }

    rem = len & 0xf;

    len &= ~(size_t)0xf;
    if (len) {
        s390x_kmo(in, len, out, adat->plat.s390x.fc,
                  &adat->plat.s390x.param.kmo_kmf);

        out += len;
        in += len;
    }

    if (rem) {
        s390x_km(adat->plat.s390x.param.kmo_kmf.cv, 16,
                 adat->plat.s390x.param.kmo_kmf.cv, adat->plat.s390x.fc,
                 adat->plat.s390x.param.kmo_kmf.k);

        while (rem--) {
            out[n] = in[n] ^ adat->plat.s390x.param.kmo_kmf.cv[n];
            ++n;
        }
    }

    adat->plat.s390x.res = n;
    return 1;
}

static int s390x_aes_cfb_initkey(PROV_CIPHER_CTX *dat,
                                 const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    adat->plat.s390x.fc |= 16 << 24;   /* 16 bytes cipher feedback */
    if (!dat->enc)
        adat->plat.s390x.fc |= S390X_DECRYPT;

    adat->plat.s390x.res = 0;
    memcpy(adat->plat.s390x.param.kmo_kmf.cv, dat->iv, dat->blocksize);
    memcpy(adat->plat.s390x.param.kmo_kmf.k, key, keylen);
    return 1;
}

static int s390x_aes_cfb_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
                                   const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    int n = adat->plat.s390x.res;
    int rem;
    unsigned char tmp;

    while (n && len) {
        tmp = *in;
        *out = adat->plat.s390x.param.kmo_kmf.cv[n] ^ tmp;
        adat->plat.s390x.param.kmo_kmf.cv[n] = dat->enc ? *out : tmp;
        n = (n + 1) & 0xf;
        --len;
        ++in;
        ++out;
    }

    rem = len & 0xf;

    len &= ~(size_t)0xf;
    if (len) {
        s390x_kmf(in, len, out, adat->plat.s390x.fc,
                  &adat->plat.s390x.param.kmo_kmf);

        out += len;
        in += len;
    }

    if (rem) {
        s390x_km(adat->plat.s390x.param.kmo_kmf.cv, 16,
                 adat->plat.s390x.param.kmo_kmf.cv,
                 S390X_AES_FC(dat->keylen), adat->plat.s390x.param.kmo_kmf.k);

        while (rem--) {
            tmp = in[n];
            out[n] = adat->plat.s390x.param.kmo_kmf.cv[n] ^ tmp;
            adat->plat.s390x.param.kmo_kmf.cv[n] = dat->enc ? out[n] : tmp;
            ++n;
        }
    }

    adat->plat.s390x.res = n;
    return 1;
}

static int s390x_aes_cfb8_initkey(PROV_CIPHER_CTX *dat,
                                  const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    adat->plat.s390x.fc |= 1 << 24;   /* 1 byte cipher feedback */
    if (!dat->enc)
        adat->plat.s390x.fc |= S390X_DECRYPT;

    memcpy(adat->plat.s390x.param.kmo_kmf.cv, dat->iv, dat->blocksize);
    memcpy(adat->plat.s390x.param.kmo_kmf.k, key, keylen);
    return 1;
}

static int s390x_aes_cfb8_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
                                    const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    s390x_kmf(in, len, out, adat->plat.s390x.fc,
              &adat->plat.s390x.param.kmo_kmf);
    return 1;
}

#define PROV_CIPHER_HW_declare(mode)                                           \
static const PROV_CIPHER_HW s390x_aes_##mode = {                               \
    s390x_aes_##mode##_initkey,                                                \
    s390x_aes_##mode##_cipher_hw                                               \
};
#define PROV_CIPHER_HW_select(mode)                                            \
if ((keybits == 128 && S390X_aes_128_##mode##_CAPABLE)                         \
     || (keybits == 192 && S390X_aes_192_##mode##_CAPABLE)                     \
     || (keybits == 256 && S390X_aes_256_##mode##_CAPABLE))                    \
    return &s390x_aes_##mode;

