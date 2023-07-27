################################################################################
# webview-builder
################################################################################

FROM ubuntu:jammy as webview-builder

ENV LANG=C.UTF-8 LANGUAGE=C LC_ALL=C.UTF-8
ARG DEBIAN_FRONTEND=noninteractive

ARG LIBWPE=libwpe-1.14.1
ARG WPEWEBKIT=wpewebkit-2.38.6

ARG MESON_BUILDTYPE=release
ARG CMAKE_BUILDTYPE=Release

RUN apt-get update -qq && \
    apt-get upgrade -qq && \
    apt-get install -qq --no-install-recommends \
        python3-pip wget cmake pkg-config gperf xz-utils patch \
        build-essential libegl1-mesa-dev libxkbcommon-dev libatk-bridge2.0-dev \
        libatk1.0-dev libcairo2-dev libepoxy-dev libgbm-dev libgcrypt20-dev \
        libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev libharfbuzz-dev \
        libicu-dev libjpeg-dev liblcms2-dev libopenjp2-7-dev libsoup2.4-dev \
        libsqlite3-dev libsystemd-dev libtasn1-6-dev libwayland-dev \
        libwebp-dev libwoff-dev libxslt1-dev wayland-protocols libglib2.0-dev \
        libgtk-3-dev libfontconfig1-dev libfreetype6-dev libhyphen-dev \
        libmanette-0.2-dev libnotify-dev libxml2-dev libgudev-1.0-dev bison \
        flex libpng-dev libxt-dev libenchant-2-dev libsecret-1-dev libgl-dev \
        libgles-dev libjavascriptcoregtk-4.1-0 gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good gstreamer1.0-gl libgl1-mesa-dri patchelf \
        unifdef ruby-dev libgstreamer-plugins-bad1.0-dev \
        gstreamer1.0-plugins-bad && \
    pip3 install meson ninja

WORKDIR /root
COPY ./wpewebkit-patches ./wpewebkit-patches
RUN wget https://wpewebkit.org/releases/$LIBWPE.tar.xz && \
    tar -xf $LIBWPE.tar.xz && \
    wget https://wpewebkit.org/releases/$WPEWEBKIT.tar.xz && \
    tar -xf $WPEWEBKIT.tar.xz && \
    for PATCH in ./wpewebkit-patches/*.patch; do patch -d $WPEWEBKIT -f -l -p1 < $PATCH; done

RUN cd /root/$LIBWPE && \
    meson setup build --buildtype=$MESON_BUILDTYPE --prefix=/usr && \
    ninja -C build install && \
    cd /root/$WPEWEBKIT && \
    cmake -S. -Bbuild -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
          -DCMAKE_BUILD_TYPE=$CMAKE_BUILDTYPE -DCMAKE_INSTALL_PREFIX=/usr \
          -DPORT=WPE -DENABLE_ACCESSIBILITY=OFF -DENABLE_BUBBLEWRAP_SANDBOX=OFF -DENABLE_DOCUMENTATION=OFF \
          -DENABLE_GAMEPAD=OFF -DENABLE_INTROSPECTION=OFF -DENABLE_JOURNALD_LOG=OFF \
          -DENABLE_OFFSCREEN_CANVAS=ON -DENABLE_OFFSCREEN_CANVAS_IN_WORKERS=ON \
          -DENABLE_PDFJS=OFF -DENABLE_WEBDRIVER=OFF -DUSE_SOUP2=ON -DUSE_AVIF=OFF \
          -DENABLE_WPE_QT_API=OFF -DENABLE_COG=OFF -DENABLE_MINIBROWSER=OFF && \
    ninja -C build install

COPY ./backend ./backend
RUN cd /root/backend && \
    meson setup build --buildtype=$MESON_BUILDTYPE --prefix=/usr && \
    ninja -C build install && \
    cd /usr/lib/x86_64-linux-gnu && \
    ln -s libwpebackend-offscreen.so libWPEBackend-default.so

################################################################################
# webview-sample
################################################################################

FROM ubuntu:jammy as webview-sample

ENV LANG=C.UTF-8 LANGUAGE=C LC_ALL=C.UTF-8
ARG DEBIAN_FRONTEND=noninteractive

ARG MESON_BUILDTYPE=release

RUN apt-get update -qq && \
    apt-get upgrade -qq && \
    apt-get install -qq --no-install-recommends \
        python3-pip pkg-config build-essential \
        gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
        libgstreamer-gl1.0-0 libepoxy0 libharfbuzz-icu0 libjpeg8 \
        libxslt1.1 liblcms2-2 libopenjp2-7 libwebpdemux2 libwoff1 libcairo2 \
        libglib2.0-dev libsoup2.4-dev libxkbcommon-dev libegl1-mesa-dev && \
    pip3 install meson ninja

COPY --from=webview-builder /usr/include/wpe-1.0 /usr/include/wpe-1.0
COPY --from=webview-builder /usr/include/wpe-webkit-1.0 /usr/include/wpe-webkit-1.0
COPY --from=webview-builder /usr/include/wpebackend-offscreen.h /usr/include/
COPY --from=webview-builder /usr/lib/x86_64-linux-gnu/wpe-webkit-1.0 /usr/lib/x86_64-linux-gnu/wpe-webkit-1.0
COPY --from=webview-builder /usr/lib/x86_64-linux-gnu/pkgconfig/wpe*.pc /usr/lib/x86_64-linux-gnu/pkgconfig/
COPY --from=webview-builder /usr/lib/x86_64-linux-gnu/libwpe-1.0.so* /usr/lib/x86_64-linux-gnu/
COPY --from=webview-builder /usr/lib/x86_64-linux-gnu/libwpebackend-offscreen.so /usr/lib/x86_64-linux-gnu/
COPY --from=webview-builder /usr/lib/x86_64-linux-gnu/libWPEBackend-default.so /usr/lib/x86_64-linux-gnu/
COPY --from=webview-builder /usr/lib/x86_64-linux-gnu/libWPEWebKit-1.0.so* /usr/lib/x86_64-linux-gnu/
COPY --from=webview-builder /usr/libexec/wpe-webkit-1.0 /usr/libexec/wpe-webkit-1.0

WORKDIR /root
COPY ./sample ./sample
RUN cd /root/sample && \
    meson setup build --buildtype=$MESON_BUILDTYPE && \
    ninja -C build

ENTRYPOINT [ "/root/sample/build/webview-sample" ]
