FROM ubuntu:24.04 as qemu-builder

RUN apt update && apt upgrade -y
RUN apt install --no-install-recommends -y \
            bash \
            bc \
            bison \
            bzip2 \
            ca-certificates \
            cbindgen \
            curl \
            findutils \
            flex \
            gcc \
            git \
            libc6-dev \
            libcap-ng-dev \
            libfdt-dev \
            libffi-dev \
            libglib2.0-dev \
            libpixman-1-dev \
            libseccomp-dev \
            libssl-dev \
            locales \
            make \
            meson \
            ninja-build \
            pkgconf \
            python3 \
            python3-venv \
            sed \
            tar \
            unzip

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
RUN . "$HOME/.cargo/env" && cargo install cargo-c

RUN git config --global user.email "you@example.com"
RUN git config --global user.name "Your Name"

RUN git clone https://github.com/microsoft/igvm.git
RUN cd igvm \
  && git remote add jb https://github.com/jethrogb/igvm.git \
  && git fetch jb \
  && git cherry-pick 7e42ce5ad9c21bf3230a0d269ca146057c2603e9 \
  && git cherry-pick c99404e194760233bfd4b2770145ab34d4c086ec \
  && git cherry-pick 29c64ac42c29317430f02bc30857c79cc9d87f40
RUN cd igvm && . "$HOME/.cargo/env" && cargo build
RUN cd igvm && . "$HOME/.cargo/env" && make -f igvm_c/Makefile install

RUN cp igvm/target/debug/libigvm.a /usr/local/lib/
RUN cp igvm/igvm_c/include/*.h /usr/local/include/

RUN git clone https://github.com/fortanix/qemu.git --depth 1 --branch cvm-fixes

ENV PKG_CONFIG_PATH /usr/lib64/pkgconfig

WORKDIR qemu
RUN mkdir build && cd build && ../configure \
  --prefix=/opt/qemu \
  --target-list=x86_64-softmmu \
  --without-default-features \
  --enable-attr \
  --enable-cap-ng \
  --enable-gio \
  --enable-guest-agent \
  --enable-igvm \
  --enable-kvm \
  --enable-l2tpv3 \
  --enable-libvduse \
  --enable-malloc-trim \
  --enable-membarrier \
  --enable-multiprocess \
  --enable-passt \
  --enable-plugins \
  --enable-seccomp \
  --enable-selinux \
  --enable-slirp \
  --enable-stack-protector \
  --enable-vhost-kernel \
  --enable-vhost-net \
  --enable-vhost-user \
  --enable-tools
RUN cd build && make -j$(nproc)
RUN cd build && make install

FROM ubuntu:24.04
ENV LD_LIBRARY_PATH /usr/lib64
RUN mkdir -p /var/local/run

RUN apt update && apt upgrade -y && apt-get install -y \
  libfdt1 \
  libglibd-2.0-0 \
  libpixman-1-0 \
  libseccomp2 \
  libslirp0

COPY --from=qemu-builder /opt/qemu /opt/qemu
ENV PATH="/opt/qemu/bin:${PATH}"

COPY --from=qemu-builder /usr/lib64/libigvm.so /usr/lib64/libigvm.so
COPY --from=qemu-builder /usr/lib64/libigvm.so.0.5 /usr/lib64/libigvm.so.0.5
COPY --from=qemu-builder /usr/lib64/pkgconfig /usr/lib64/pkgconfig
COPY --from=qemu-builder /usr/lib64/pkgconfig/igvm.pc /usr/lib64/pkgconfig/igvm.pc
COPY --from=qemu-builder /usr/lib64/libigvm.a /usr/lib64/libigvm.a
COPY --from=qemu-builder /usr/lib64/libigvm.so.0.5.0 /usr/lib64/libigvm.so.0.5.0

