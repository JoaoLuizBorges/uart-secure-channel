#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <stdint.h>
#include <stdbool.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ccm.h"

extern uint8_t psk[32];

typedef struct {

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ccm_context ccm;

    uint8_t session_key[16];
    uint32_t tx_seq;
    uint32_t rx_seq;

} crypt_context_t;

void cryp_init(crypt_context_t *ctx);

int generate_nonce( mbedtls_ctr_drbg_context *ctr_drbg,
					uint8_t *nonce,
					size_t nonce_len);

int derive_session_key( crypt_context_t *ctx,
						const uint8_t *psk, size_t psk_len,
						const uint8_t *client_nonce, size_t client_nonce_len,
						const uint8_t *server_nonce, size_t server_nonce_len);

int encrypt_frame(crypt_context_t *ctx,
                  const uint8_t *plaintext, size_t plaintext_len,
                  uint8_t *out, size_t *out_len);

int decrypt_frame(crypt_context_t *ctx,
                  const uint8_t *in, size_t in_len,
                  uint8_t *plaintext, size_t *plaintext_len);

#endif // ENCRYPT_H
