#include "esp_err.h"
#include "esp_log.h"
#include "mbedtls/ccm.h"
#include "mbedtls/cipher.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/hkdf.h"
#include <mbedtls/aes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include <string.h>

#include "include/encrypt.h"
#include "include/secure_keys.h"

void cryp_init(crypt_context_t *ctx) {
	
	mbedtls_ctr_drbg_init (&ctx->ctr_drbg);
	mbedtls_entropy_init (&ctx->entropy);
	mbedtls_ccm_init(&ctx->ccm);
	
	ctx->tx_seq = 0;
	ctx->rx_seq = 0;
	
	const char *pers = "uart_secure";
	
	ESP_ERROR_CHECK(mbedtls_ctr_drbg_seed(
		&ctx->ctr_drbg,
		mbedtls_entropy_func,
		&ctx->entropy,
		(const unsigned char *)pers,
		strlen(pers)
		)
	);
	
	ESP_LOGI(TAG, "Criptografia inicializada");
}

int generate_nonce(
    mbedtls_ctr_drbg_context *ctr_drbg,
    uint8_t *nonce,
    size_t nonce_len
) {	
	
    return mbedtls_ctr_drbg_random(ctr_drbg, nonce, nonce_len);
}

int derive_session_key(
	crypt_context_t *ctx,
	const uint8_t *psk, size_t psk_len,
	const uint8_t *client_nonce, size_t client_nonce_len,
	const uint8_t *server_nonce, size_t server_nonce_len
) {
	
	static uint8_t salt[32];
	

	
	memcpy(salt, client_nonce, client_nonce_len);
	memcpy(salt + client_nonce_len, server_nonce, server_nonce_len);
	
	const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	
	return mbedtls_hkdf(
		md,
		salt, client_nonce_len + server_nonce_len,
		psk, psk_len,
		(const unsigned char*)"UART-SECURE-V1",
		strlen("UART-SECURE-V1"),
		ctx->session_key,
		sizeof(ctx->session_key)
	);
}

int encrypt_frame(crypt_context_t *ctx,
                  const uint8_t *plaintext, size_t plaintext_len,
                  uint8_t *out, size_t *out_len) {
	
	
	static uint8_t iv[12];
	static uint8_t tag[16];
	uint32_t seq = ctx->tx_seq++;
		
	memset(iv, 0, sizeof(iv));
    memcpy(iv, &seq, sizeof(seq));
        
    memcpy(out, &seq, sizeof(seq));
    memcpy(out + 4, iv, sizeof(iv));

	int ret = mbedtls_ccm_setkey(
		&ctx->ccm, 
		MBEDTLS_CIPHER_ID_AES, 
		ctx->session_key, 
		128
	);
	if (ret != 0) return ret;
	
	ret = mbedtls_ccm_encrypt_and_tag(
	    &ctx->ccm,
	    plaintext_len,
	    iv, sizeof(iv),
	    out, 4,
	    plaintext,
	    out + 4 + sizeof(iv),
	    tag, sizeof(tag)
	);	
	
	if (ret != 0) return ret; 
		
	memcpy(out + 4 + sizeof(iv) + plaintext_len, tag, sizeof(tag));

    *out_len = 4 + sizeof(iv) + plaintext_len + sizeof(tag);

    return 0;
				
}

int decrypt_frame(crypt_context_t *ctx,
                  const uint8_t *in, size_t in_len,
                  uint8_t *plaintext, size_t *plaintext_len) {
		
    if (in_len < (4 + 12 + 16))
        return -1;

    uint32_t seq;
    memcpy(&seq, in, sizeof(seq));

    if (seq <= ctx->rx_seq)
        return -2;  // replay ou reorder

    const uint8_t *iv = in + 4;
    const uint8_t *ciphertext = in + 4 + 12;
    size_t cipher_len = in_len - (4 + 12 + 16);
    const uint8_t *tag = in + in_len - 16;
	
	int ret = mbedtls_ccm_setkey(
        &ctx->ccm,
        MBEDTLS_CIPHER_ID_AES,
        ctx->session_key,
        128
    );
    if (ret != 0) return -4;
    
    ret = mbedtls_ccm_auth_decrypt(
	    &ctx->ccm,
	    cipher_len,
	    iv, 12,
	    in, 4,
	    ciphertext,
	    plaintext,
	    tag, 16
	);

    if (ret != 0)
        return -3;  // autenticação falhou

    ctx->rx_seq = seq;
    *plaintext_len = cipher_len;

    return 0;
}