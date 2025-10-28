/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * This source file is part of the Cangjie project, licensed under Apache-2.0
 * with Runtime Library Exception.
 *
 * See https://cangjie-lang.cn/pages/LICENSE for license information.
 */

#include <openssl/bn.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdlib.h>
#include <string.h>
#include "provider.h"
#include "opensslSymbols.h"
#include "securec.h"
#include "api.h"

__attribute__((visibility("hidden"))) unsigned int GetEcOrderBitsFromGroup(const char* groupName)
{
    if (!groupName) {
        return 0;
    }
    if (!strcasecmp(groupName, "P-256") || !strcasecmp(groupName, "p256") || !strcasecmp(groupName, "prime256v1") || !strcasecmp(groupName, "secp256r1")) {
        return 256;
    }
    if (!strcasecmp(groupName, "P-384") || !strcasecmp(groupName, "p384") || !strcasecmp(groupName, "secp384r1")) {
        return 384;
    }
    if (!strcasecmp(groupName, "P-521") || !strcasecmp(groupName, "p521") || !strcasecmp(groupName, "secp521r1")) {
        return 521;
    }
    if (!strcasecmp(groupName, "secp256k1")) {
        return 256;
    }

    return 0;
}

__attribute__((visibility("hidden"))) inline int8_t GetKeylessLogLevel()
{
    static int8_t cached = -1;
    if (cached < 0) {
        const char* e = getenv("CJ_KEYLESS_DEBUG");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

__attribute__((visibility("hidden"))) size_t KeylessKeyGetSize(const void* keyData)
{
    const KeylessKey* k = keyData;
    if (!k) {
        return 0;
    }
    if (k->type == KEYLESS_KEY_TYPE_RSA) {
        return k->nLen;
    }
    /**
    * For EC keys, 80-byte upper bound, which is conservative enough for all supported curves
    * and keeps OpenSSL’s size probes satisfied.
    */
    if (k->type == KEYLESS_KEY_TYPE_EC) {
        return 80;
    }
    return 0;
}

__attribute__((visibility("hidden"))) const unsigned char* KeylessKeyGetN(const void* keyData)
{
    const KeylessKey* k = keyData;
    return (k && k->type == KEYLESS_KEY_TYPE_RSA) ? k->n : NULL;
}

__attribute__((visibility("hidden"))) size_t KeylessKeyGetNLen(const void* keyData)
{
    const KeylessKey* k = keyData;
    return (k && k->type == KEYLESS_KEY_TYPE_RSA) ? k->nLen : 0;
}

/**
 * Computes the SHA-256 hash of the issuer and serial number of the given X509 certificate,
 * and outputs the result as a hexadecimal string.
 *
 * @param crt      Pointer to the X509 certificate.
 * @param out_hex  Output buffer to store the resulting SHA-256 hash in hexadecimal format.
 *                 Must be at least 65 bytes to accommodate the 64-character hash and null terminator.
 * @return         0 on success, non-zero on failure.
 */
__attribute__((visibility("hidden"))) int CertIssuerSerialSha256Hex(const X509* crt, char out_hex[65])
{
    if (!crt || !out_hex) {
        return 0;
    }

    DynMsg* dynMsg = KeylessProviderGetThreadDynMsg();
    const X509_NAME* issuer = DYN_X509_get_issuer_name((X509*)crt, dynMsg);
    const ASN1_INTEGER* serial = DYN_X509_get0_serialNumber((X509*)crt, dynMsg);
    if (!issuer || !serial) {
        KeylessCheckDynMsg(dynMsg, "X509_get_issuer_name or X509_get0_serialNumber");
        return 0;
    }

    unsigned char *der_issuer = NULL, *der_serial = NULL;
    int len_issuer = DYN_i2d_X509_NAME((X509_NAME*)issuer, &der_issuer, dynMsg);
    int len_serial = DYN_i2d_ASN1_INTEGER((ASN1_INTEGER*)serial, &der_serial, dynMsg);
    if (len_issuer <= 0 || len_serial <= 0) {
        DYN_OPENSSL_secure_free(der_issuer, dynMsg);
        DYN_OPENSSL_secure_free(der_serial, dynMsg);
        return 0;
    }

    size_t total = (size_t)len_issuer + (size_t)len_serial;
    unsigned char* cat = DYN_OPENSSL_secure_malloc(total, dynMsg);
    if (!cat) {
        DYN_OPENSSL_secure_free(der_issuer, dynMsg);
        DYN_OPENSSL_secure_free(der_serial, dynMsg);
        return 0;
    }
    (void)memcpy_s(cat, total, der_issuer, len_issuer);
    (void)memcpy_s(cat + len_issuer, total - len_issuer, der_serial, len_serial);

    unsigned char dig[32]; // 32： SHA256(256bit) / 8 = 32byte
    if (!DYN_EVP_Q_digest(NULL, "SHA256", NULL, cat, total, dig, NULL, dynMsg)) {
        DYN_OPENSSL_secure_free(cat, dynMsg);
        DYN_OPENSSL_secure_free(der_issuer, dynMsg);
        DYN_OPENSSL_secure_free(der_serial, dynMsg);
        return 0;
    }

    static const char* hex = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out_hex[i * 2] = hex[(dig[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hex[dig[i] & 0xF];
    }
    out_hex[64] = '\0'; // SHA-256 is 32 bytes -> 64 hex chars; index 64 is the NUL terminator

    DYN_OPENSSL_secure_free(cat, dynMsg);
    DYN_OPENSSL_secure_free(der_issuer, dynMsg);
    DYN_OPENSSL_secure_free(der_serial, dynMsg);
    KeylessCheckDynMsg(dynMsg, "CertIssuerSerialSha256Hex");
    return 1;
}

/**
 * @brief Compute the SHA-256 fingerprint of an X.509 certificate and return it as a hexadecimal string.
 *
 * Computes the SHA-256 digest of the provided certificate and encodes it as a
 * null-terminated hexadecimal string without separators (64 hex characters).
 *
 * @param crt Pointer to an OpenSSL X509 certificate. Must not be NULL.
 *
 * @return A newly allocated C string containing the hex-encoded SHA-256 digest on success;
 *         NULL on failure (e.g., invalid input, digest computation error, or memory allocation failure).
 *
 * @note The caller owns the returned string and is responsible for freeing it with free().
 */
__attribute__((visibility("hidden"))) char* GetCertSha256Hex(const X509* crt)
{
    const size_t bufSize = 65; // 64 hex chars + null terminator
    char buf[bufSize];

    if (!CertIssuerSerialSha256Hex(crt, buf)) {
        return NULL;
    }

    DynMsg* dynMsg = KeylessProviderGetThreadDynMsg();
    char* out = DYN_OPENSSL_secure_malloc(bufSize, dynMsg);
    KeylessCheckDynMsg(dynMsg, "GetCertSha256Hex");
    if (!out) {
        return NULL;
    }
    (void)memcpy_s(out, bufSize, buf, bufSize);
    return out;
}

extern char* CJ_TLS_GetCertId(const unsigned char* der, size_t len)
{
    const unsigned char* p = der;
    DynMsg* dynMsg = KeylessProviderGetThreadDynMsg();
    X509* cert = DYN_d2i_X509(NULL, &p, (long)len, dynMsg);
    KeylessCheckDynMsg(dynMsg, "CJ_TLS_GetCertId");
    if (!cert) {
        return NULL;
    }
    char* out = GetCertSha256Hex(cert);
    DYN_X509_free(cert, dynMsg);
    KeylessCheckDynMsg(dynMsg, "CJ_TLS_GetCertId");
    return out;
}
