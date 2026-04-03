ssl_dep = dependency('libssl')
crypto_dep = dependency('libcrypto')
dependencies = [ssl_dep, crypto_dep]

external_inc = include_directories('external')
io_inc = include_directories('io')
logging_inc = include_directories('logging')
utils_inc = include_directories('utils')

inc_dirs = [
    external_inc,
    io_inc,
    logging_inc,
    utils_inc
]

executable('client', 'client.cpp',
    include_directories: inc_dirs,
    dependencies: dependencies
)

executable('chat-server', 'chat-server.cpp',
    include_directories: inc_dirs,
    dependencies: dependencies
)

executable('echo-server', 'echo-server.cpp',
    include_directories: inc_dirs,
    dependencies: dependencies
)
