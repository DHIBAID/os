FROM randomdude/gcc-cross-x86_64-elf

RUN apt update
RUN apt upgrade -y

RUN apt install -y nasm 
RUN apt install -y xorriso
RUN apt install -y grub-pc-bin
RUN apt install -y grub-common

VOLUME /root/env
WORKDIR /root/env

