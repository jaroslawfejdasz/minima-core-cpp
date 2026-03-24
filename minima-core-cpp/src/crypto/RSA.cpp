/**
 * RSA.cpp — RSA-1024 SHA256withRSA
 * Java ref: org.minima.utils.encrypt.SignVerify (SIGN_ALGO="SHA256withRSA", KEY_SIZE=1024)
 *
 * MINIMA_OPENSSL → real OpenSSL EVP implementation
 * otherwise      → stub that throws (CI without OpenSSL)
 */
#include "RSA.hpp"

#ifdef MINIMA_OPENSSL
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/x509.h>

namespace minima::crypto {

// Private key: traditional DER (RSAPrivateKey or PKCS#8)
static EVP_PKEY* loadPrivKey(const MiniData& d) {
    const uint8_t* p = d.bytes().data();
    long len = (long)d.bytes().size();
    // Try PKCS#8 PrivateKeyInfo first, fall back to traditional RSAPrivateKey
    EVP_PKEY* k = d2i_AutoPrivateKey(nullptr, &p, len);
    return k;
}

// Public key: X.509 SubjectPublicKeyInfo DER
static EVP_PKEY* loadPubKey(const MiniData& d) {
    const uint8_t* p = d.bytes().data();
    long len = (long)d.bytes().size();
    // d2i_PUBKEY_ex available in OpenSSL 3.x
    return d2i_PUBKEY_ex(nullptr, &p, len, nullptr, nullptr);
}

RSAKeyPair RSA::generateKeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) throw std::runtime_error("RSA keygen: EVP_PKEY_CTX_new_id failed");
    if (EVP_PKEY_keygen_init(ctx) <= 0)
        throw std::runtime_error("RSA keygen: init failed");
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024) <= 0)
        throw std::runtime_error("RSA keygen: set_bits failed");

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
        throw std::runtime_error("RSA keygen: keygen failed");
    EVP_PKEY_CTX_free(ctx);

    // Private key → traditional RSAPrivateKey DER
    int privLen = i2d_PrivateKey(pkey, nullptr);
    std::vector<uint8_t> privB(privLen);
    uint8_t* pp = privB.data();
    i2d_PrivateKey(pkey, &pp);

    // Public key → X.509 SubjectPublicKeyInfo DER
    int pubLen = i2d_PUBKEY(pkey, nullptr);
    std::vector<uint8_t> pubB(pubLen);
    uint8_t* qp = pubB.data();
    i2d_PUBKEY(pkey, &qp);

    EVP_PKEY_free(pkey);
    return { MiniData(privB), MiniData(pubB) };
}

MiniData RSA::sign(const MiniData& privateKey, const MiniData& message) {
    EVP_PKEY* pkey = loadPrivKey(privateKey);
    if (!pkey) throw std::runtime_error("RSA sign: bad private key");

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
        { EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey); throw std::runtime_error("RSA sign: DigestSignInit"); }
    if (EVP_DigestSignUpdate(ctx, message.bytes().data(), message.bytes().size()) <= 0)
        { EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey); throw std::runtime_error("RSA sign: DigestSignUpdate"); }

    size_t sigLen = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sigLen);
    std::vector<uint8_t> sig(sigLen);
    if (EVP_DigestSignFinal(ctx, sig.data(), &sigLen) <= 0)
        { EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey); throw std::runtime_error("RSA sign: DigestSignFinal"); }
    sig.resize(sigLen);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return MiniData(sig);
}

bool RSA::verify(const MiniData& publicKey,
                 const MiniData& message,
                 const MiniData& signature) {
    EVP_PKEY* pkey = loadPubKey(publicKey);
    if (!pkey) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
        { EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey); return false; }
    if (EVP_DigestVerifyUpdate(ctx, message.bytes().data(), message.bytes().size()) <= 0)
        { EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey); return false; }

    int r = EVP_DigestVerifyFinal(ctx,
        signature.bytes().data(), signature.bytes().size());
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return r == 1;
}

} // namespace minima::crypto

#else
namespace minima::crypto {

RSAKeyPair RSA::generateKeyPair() {
    throw std::runtime_error("RSA requires -DMINIMA_OPENSSL");
}
MiniData RSA::sign(const MiniData&, const MiniData&) {
    throw std::runtime_error("RSA requires -DMINIMA_OPENSSL");
}
bool RSA::verify(const MiniData&, const MiniData&, const MiniData&) {
    throw std::runtime_error("RSA requires -DMINIMA_OPENSSL");
}

} // namespace minima::crypto
#endif
