Lua 5.4.9 is the unchanged archive from https://www.lua.org/ftp/lua-5.4.9.tar.gz.
SHA-256: `2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6`.

The builder patches only its extracted configuration under `target/`: 32-bit
numbers, a 512-entry Lua stack, a 20-call C nesting limit, and a seed that needs
no clock/system call. Its MIT license is in LICENSE.lua and the source archive.

The firmware also links the cross-toolchain's newlib (LICENSE.newlib) and GCC's
libgcc under the GCC Runtime Library Exception. Neither the vendor stock
firmware nor the native build is a replacement for those upstream sources.
