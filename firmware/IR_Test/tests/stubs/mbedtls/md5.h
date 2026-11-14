/* [V1.2 新增] 主機以 OpenSSL 真實 MD5 對應 MCU mbedTLS API。 */
#ifndef TEST_MD5_H
#define TEST_MD5_H
#include <openssl/evp.h>
struct mbedtls_md5_context { EVP_MD_CTX *value = nullptr; };
inline void mbedtls_md5_init(mbedtls_md5_context *ctx) { ctx->value = nullptr; }
inline void mbedtls_md5_free(mbedtls_md5_context *ctx) { EVP_MD_CTX_free(ctx->value); ctx->value = nullptr; }
inline int mbedtls_md5_starts(mbedtls_md5_context *ctx) {
    ctx->value = EVP_MD_CTX_new();
    return ctx->value != nullptr && EVP_DigestInit_ex(ctx->value, EVP_md5(), nullptr) == 1 ? 0 : -1;
}
inline int mbedtls_md5_update(mbedtls_md5_context *ctx, const unsigned char *data, size_t size) {
    return EVP_DigestUpdate(ctx->value, data, size) == 1 ? 0 : -1;
}
inline int mbedtls_md5_finish(mbedtls_md5_context *ctx, unsigned char out[16]) {
    unsigned n = 0; return EVP_DigestFinal_ex(ctx->value, out, &n) == 1 && n == 16U ? 0 : -1;
}
#endif
