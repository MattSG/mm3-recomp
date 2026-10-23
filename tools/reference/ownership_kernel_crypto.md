# Kernel crypto ownership review

Full kernel_crypto.c source read at fork 9dc9f370bc0f23b605f1a3783e3cd91326abb24e. Algorithms, key and
message inputs, RSA1 headers and public service entry points are generic Xbox
runtime mechanisms. No title identifiers, captured title keys or address tables
were found. Dead Windows BCrypt wrapper and unsupported exact-compatibility
claims were removed; historical material lives in legacy_kernel_crypto_claims.md.

Existing standalone regression passes on Windows and Linux. It covers retail
RSA verification, negative signatures/headers, PKCS1 forms, public exponentiation
hash vectors and two-key DES ECB/CBC including in-place decryption and guarded
16-byte key reads. These tests do not certify all SHA/RC4/HMAC paths or full
hardware compatibility.

Generic implementation remains incomplete: private-key decryption returns
failure, CryptService and UpdateCrypto are no-ops. Modular exponentiation has
no zero-modulus validation; length arithmetic and maximal input boundaries need
focused review. This ownership read is not certification of those behaviors.
