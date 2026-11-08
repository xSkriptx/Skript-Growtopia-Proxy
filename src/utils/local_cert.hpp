#pragma once

// Generates a per-install, name-constrained self-signed certificate for the local
// HTTPS server. Nothing here is shipped: every user gets their own private key.

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <filesystem>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace utils::local_cert {

namespace detail {
template <typename T, void (*Free)(T*)>
struct Deleter {
    void operator()(T* p) const { if (p) Free(p); }
};
using X509Ptr = std::unique_ptr<X509, Deleter<X509, X509_free>>;
using PKeyPtr = std::unique_ptr<EVP_PKEY, Deleter<EVP_PKEY, EVP_PKEY_free>>;
using BioPtr = std::unique_ptr<BIO, int (*)(BIO*)>;

inline bool add_ext(X509* cert, int nid, const char* value)
{
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value));
    if (!ext) {
        return false;
    }

    const int ok = X509_add_ext(cert, ext, -1);
    X509_EXTENSION_free(ext);
    return ok == 1;
}
} // namespace detail

// Creates cert_path/key_path if either is missing. Returns true when both usable files exist.
inline bool ensure(const std::string& cert_path, const std::string& key_path)
{
    namespace fs = std::filesystem;
    if (fs::exists(cert_path) && fs::exists(key_path)) {
        return true;
    }

    spdlog::info("Generating a local HTTPS certificate (first run)...");

    std::unique_ptr<BIGNUM, detail::Deleter<BIGNUM, BN_free>> e{ BN_new() };
    std::unique_ptr<RSA, detail::Deleter<RSA, RSA_free>> rsa{ RSA_new() };
    if (!e || !rsa || !BN_set_word(e.get(), RSA_F4) || !RSA_generate_key_ex(rsa.get(), 2048, e.get(), nullptr)) {
        spdlog::error("Certificate generation failed (RSA key)");
        return false;
    }

    detail::PKeyPtr pkey{ EVP_PKEY_new() };
    if (!pkey || !EVP_PKEY_assign_RSA(pkey.get(), rsa.get())) {
        spdlog::error("Certificate generation failed (EVP key)");
        return false;
    }
    rsa.release(); // owned by pkey now

    detail::X509Ptr cert{ X509_new() };
    if (!cert) {
        return false;
    }

    unsigned char serial[8]{};
    RAND_bytes(serial, sizeof(serial));
    serial[0] &= 0x7F; // keep it positive
    std::unique_ptr<BIGNUM, detail::Deleter<BIGNUM, BN_free>> serial_bn{ BN_bin2bn(serial, sizeof(serial), nullptr) };
    BN_to_ASN1_INTEGER(serial_bn.get(), X509_get_serialNumber(cert.get()));

    X509_set_version(cert.get(), 2);
    X509_gmtime_adj(X509_get_notBefore(cert.get()), -3600);
    X509_gmtime_adj(X509_get_notAfter(cert.get()), 60L * 60 * 24 * 3650);
    X509_set_pubkey(cert.get(), pkey.get());

    X509_NAME* name = X509_get_subject_name(cert.get());
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Skript Proxy Local"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("www.growtopia1.com"), -1, -1, 0);
    X509_set_issuer_name(cert.get(), name);

    const bool ext_ok =
        detail::add_ext(cert.get(), NID_basic_constraints, "critical,CA:TRUE") &&
        detail::add_ext(cert.get(), NID_key_usage, "critical,digitalSignature,keyEncipherment,keyCertSign") &&
        detail::add_ext(cert.get(), NID_ext_key_usage, "serverAuth") &&
        detail::add_ext(cert.get(), NID_name_constraints, "critical,permitted;DNS:growtopia1.com,permitted;DNS:growtopia2.com") &&
        detail::add_ext(cert.get(), NID_subject_alt_name,
                        "DNS:growtopia1.com,DNS:www.growtopia1.com,DNS:growtopia2.com,DNS:www.growtopia2.com") &&
        detail::add_ext(cert.get(), NID_subject_key_identifier, "hash");
    if (!ext_ok) {
        spdlog::error("Certificate generation failed (extensions)");
        return false;
    }

    if (!X509_sign(cert.get(), pkey.get(), EVP_sha256())) {
        spdlog::error("Certificate generation failed (sign)");
        return false;
    }

    std::error_code ec;
    fs::create_directories(fs::path{ cert_path }.parent_path(), ec);

    detail::BioPtr cert_bio{ BIO_new_file(cert_path.c_str(), "wb"), BIO_free };
    detail::BioPtr key_bio{ BIO_new_file(key_path.c_str(), "wb"), BIO_free };
    if (!cert_bio || !key_bio ||
        !PEM_write_bio_X509(cert_bio.get(), cert.get()) ||
        !PEM_write_bio_PrivateKey(key_bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
        spdlog::error("Failed to write {} / {}", cert_path, key_path);
        return false;
    }

    return true;
}

// SHA-1 thumbprint (uppercase hex) of a PEM certificate, as accepted by certutil. Empty on failure.
inline std::string thumbprint(const std::string& cert_path)
{
    detail::BioPtr bio{ BIO_new_file(cert_path.c_str(), "rb"), BIO_free };
    if (!bio) {
        return {};
    }

    detail::X509Ptr cert{ PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) };
    if (!cert) {
        return {};
    }

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (!X509_digest(cert.get(), EVP_sha1(), md, &len)) {
        return {};
    }

    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned int i = 0; i < len; ++i) {
        out += hex[md[i] >> 4];
        out += hex[md[i] & 0xF];
    }
    return out;
}

} // namespace utils::local_cert
