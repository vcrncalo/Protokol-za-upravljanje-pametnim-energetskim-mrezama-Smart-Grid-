#ifndef PQC_TLS_HPP
#define PQC_TLS_HPP

#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>

#include <stdexcept>
#include <string>

namespace ssl = boost::asio::ssl;

inline void configurePqcTls(ssl::context& context)
{
    SSL_CTX* ctx = context.native_handle();

    // Koristimo iskljucivo TLS 1.3.
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1)
    {
        throw std::runtime_error(
            "Nije moguce postaviti minimalnu TLS verziju na TLS 1.3."
        );
    }

    if (SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION) != 1)
    {
        throw std::runtime_error(
            "Nije moguce postaviti maksimalnu TLS verziju na TLS 1.3."
        );
    }

    // Hibridna klasicna + post-kvantna razmjena kljuca.
    if (SSL_CTX_set1_groups_list(ctx, "X25519MLKEM768") != 1)
    {
        throw std::runtime_error(
            "X25519MLKEM768 nije podrzan."
        );
    }

    // Post-kvantni digitalni potpis.
    if (SSL_CTX_set1_sigalgs_list(ctx, "ML-DSA-44") != 1)
    {
        throw std::runtime_error(
            "ML-DSA-44 nije podrzan."
        );
    }
}

#endif
