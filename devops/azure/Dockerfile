ARG BASE_IMAGE=mcr.microsoft.com/azurelinux/base/core:3.0
FROM ${BASE_IMAGE} AS build-env

ARG VERSION=0.1
ARG BUILD_NUMBER="unknown"
ARG BUILD_DATE="unknown"

# Package version arguments
ARG CMAKE_VERSION=3.30.3-6.azl3
ARG CA_CERTIFICATES_VERSION=3.0.0-8.azl3
ARG LIBGOMP_VERSION=13.2.0-7.azl3
ARG GIT_VERSION=2.45.2-1.azl3
ARG TAR_VERSION=1.35-1.azl3
ARG CURL_VERSION=8.8.0-4.azl3
ARG ZIP_VERSION=3.0-6.azl3
ARG UNZIP_VERSION=6.0-20.azl3
ARG PKGCONF_VERSION=2.0.2-1.azl3
ARG AUTOCONF_VERSION=2.72-1.azl3
ARG BISON_VERSION=3.8.2-1.azl3
ARG FLEX_VERSION=2.6.4-7.azl3
ARG PERL_VERSION=5.38.2-508.azl3
ARG PERL_IPC_CMD_VERSION=1.04-508.azl3


LABEL version=${VERSION}
LABEL vendor="OSDU Reservoir DDMS"
LABEL description="Open ETP Server Azure Build Image"
LABEL build-number=${BUILD_NUMBER}
LABEL build-date=${BUILD_DATE}

RUN tdnf upgrade -y && \
    tdnf install -y \
        ca-certificates-${CA_CERTIFICATES_VERSION} \
        # System Build packages
        cmake-${CMAKE_VERSION} \
        ninja-build \
        libgomp-${LIBGOMP_VERSION} \
        gdb \
        # VCPKG prerequisites
        git-${GIT_VERSION} \
        build-essential \
        gcc gcc-c++ make \
        tar-${TAR_VERSION} \
        curl-${CURL_VERSION} \
        zip-${ZIP_VERSION} \
        unzip-${UNZIP_VERSION} \
        pkgconf-${PKGCONF_VERSION} \
        # libpq requires these packages
        autoconf-${AUTOCONF_VERSION} \
        bison-${BISON_VERSION} \
        flex-${FLEX_VERSION} \
        # Perl packages required for OpenSSL build
        perl-${PERL_VERSION} \
        perl-IPC-Cmd-${PERL_IPC_CMD_VERSION}

# Install VCPKG manager
ENV VCPKG_ROOT="/vcpkg"
ARG VCPKG_VERSION="ef7dbf94b9198bc58f45951adcf1f041fcbc5ea0"

RUN git clone --filter=blob:none --single-branch --no-checkout \
        https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} && \
    git -C ${VCPKG_ROOT} checkout --detach ${VCPKG_VERSION}

WORKDIR ${VCPKG_ROOT}
RUN ./bootstrap-vcpkg.sh

WORKDIR /source

# Normally this happens in the cmake command later,
# but separating it out like this improves caching performance
COPY ./vcpkg.json /source/
RUN ${VCPKG_ROOT}/vcpkg install

COPY ./CMakeLists.txt /source/
COPY ./cmake/ /source/cmake/
COPY ./config/ /source/config/
COPY ./src/ /source/src/

# Build Open-ETP-server
RUN mkdir -p /spi-test && \
    cmake -GNinja -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DCMAKE_INSTALL_PREFIX="./dist" \
        -DCODE_COVERAGE=OFF && \
    ninja -C build -j8 install && \
    ctest --test-dir build --label-regex UNIT --output-on-failure \
        --output-junit /spi-test/ctest.xml

WORKDIR /source/dist
RUN tar -czvf /source/dist.tgz .

FROM scratch AS spi-test
COPY --from=build-env /spi-test/ctest.xml /ctest.xml

FROM build-env AS acceptance-fixtures

COPY ./devops/scripts/fetch-test-fixtures.sh /usr/local/bin/fetch-test-fixtures
RUN bash /usr/local/bin/fetch-test-fixtures acceptance /fixtures

# Common non-root runtime used by the service and acceptance images.
FROM ${BASE_IMAGE} AS runtime-base

ARG VERSION=0.1
ARG BUILD_NUMBER="unknown"
ARG BUILD_DATE="unknown"
ARG TAR_VERSION=1.35-1.azl3

LABEL version="${VERSION}"
LABEL vendor="OSDU Reservoir DDMS"
LABEL description="Open ETP Server Azure Runtime Image"
LABEL build-number=${BUILD_NUMBER}
LABEL build-date=${BUILD_DATE}

RUN tdnf upgrade -y && \
    tdnf install -y tar-${TAR_VERSION} ca-certificates && \
    tdnf clean all
COPY --from=build-env /source/dist.tgz /dist.tgz
RUN tar -xzf /dist.tgz -C /usr/ && \
    rm -f /dist.tgz && \
    mkdir -p /home/openetp && \
    chown -R 1000:1000 /home/openetp

ENV HOME=/home/openetp
USER 1000:1000

EXPOSE 9002

FROM runtime-base AS acceptance
USER root
WORKDIR /suite/devops/azure/tasks
COPY --chown=1000:1000 devops/azure/tasks/etp-ssl-client-integration-tests.sh ./
COPY --from=acceptance-fixtures --chown=1000:1000 \
    /fixtures/IntegrationTestsData/ /suite/data/IntegrationTestsData/
RUN mkdir -p results && \
    chmod 0755 etp-ssl-client-integration-tests.sh && \
    chown -R 1000:1000 /suite
USER 1000:1000
ENTRYPOINT ["./etp-ssl-client-integration-tests.sh"]

FROM runtime-base AS runtime
CMD [ "openETPServer", "server", "-h" ]