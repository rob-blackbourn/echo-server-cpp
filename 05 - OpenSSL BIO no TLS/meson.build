openssl_dep = dependency('libssl')
crypto_dep = dependency('libcrypto')

executable('client', 'client.cpp',
  dependencies: [openssl_dep, crypto_dep]
)

executable('server', 'server.cpp',
  dependencies: [openssl_dep, crypto_dep]
)
