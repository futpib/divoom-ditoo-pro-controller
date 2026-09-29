# The base image and frozen package repository pin host libraries as well as the compiler.
FROM archlinux@sha256:b21322c663be387c0ed9cbc7bbbfe18e41633ad4e7b7c77cfad45f128be20040
RUN printf 'Server = https://archive.archlinux.org/repos/2026/09/28/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist \
    && pacman -Syu --noconfirm python nds32le-elf-gcc nds32le-elf-binutils nds32le-elf-newlib \
    && test "$(pacman -Q nds32le-elf-gcc)" = 'nds32le-elf-gcc 15.2.0-1' \
    && test "$(pacman -Q nds32le-elf-binutils)" = 'nds32le-elf-binutils 2.45.1-1' \
    && test "$(pacman -Q nds32le-elf-newlib)" = 'nds32le-elf-newlib 4.5.0.20241231-1'
COPY packages.sha256 /tmp/packages.sha256
RUN cd /var/cache/pacman/pkg && sha256sum -c /tmp/packages.sha256 && rm -f *.pkg.tar.zst
ENV LC_ALL=C TZ=UTC PYTHONHASHSEED=0
WORKDIR /src
CMD ["python3", "scripts/check-firmware-repro.py"]
