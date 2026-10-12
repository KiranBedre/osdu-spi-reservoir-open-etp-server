FROM gcr.io/google.com/cloudsdktool/cloud-sdk:502.0.0-debian_component_based AS build-env

# Install Core dependencies
RUN apt-get update && \
    # Define DEBIAN_FRONTEND to allow non interactive installation of some packages
    DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    # Build system packages
    # VCPKG prerequisites
        git \
        build-essential tar curl zip unzip \
        pkg-config \
    ninja-build=1.1* \
    libgomp1 \
    gdb \
    curl \
    # libpq requires these packages
    autoconf \
    bison \
    flex \
    # For code coverage
    gcovr \
    # For the beauty of the output
    figlet \
&& apt-get clean && rm -rf /var/lib/apt/lists/*

# Install Cmake (required 3.21+)
RUN curl -OL https://github.com/Kitware/CMake/releases/download/v3.22.0/cmake-3.22.0-linux-x86_64.tar.gz
RUN tar xzvf cmake-3.22.0-linux-x86_64.tar.gz -C /opt
RUN ln -s /opt/cmake-3.22.0-linux-x86_64/bin/* /usr/bin/

# Install VCPKG manager
ENV VCPKG_ROOT="$HOME/vcpkg"
RUN mkdir $VCPKG_ROOT \
    && git clone "https://github.com/Microsoft/vcpkg.git" $VCPKG_ROOT \
    && $VCPKG_ROOT/bootstrap-vcpkg.sh \
    && chmod -R 777 $VCPKG_ROOT

# Configure work dir
ADD . /source
RUN chmod 777 /source && ls -l /source/
WORKDIR /source

# Build
RUN ls -a /source/ && cmake -GNinja /source/ \
    -DCMAKE_BUILD_TYPE=Release \
    -DCODE_COVERAGE=OFF \
    -DCMAKE_INSTALL_PREFIX="./dist" \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DBUILD_OSDU_STUB_SERVER=OFF \
    -DBUILD_BENCHMARK=OFF

RUN ninja install "$@" && \
    cd ./dist && tar -czvf /source/dist.tgz .

# For image smaller for runtime 
FROM gcr.io/google.com/cloudsdktool/cloud-sdk:502.0.0-slim AS runtime-env

LABEL vendor="OSDU Reservoir DDMS"
LABEL description="Open ETP Server Runtime Image"

COPY --from=build-env /source/dist.tgz /tmp/dist.tgz
RUN tar -xzvf /tmp/dist.tgz -C /usr/ && rm -rf /tmp/dist.tgz

EXPOSE 9002

RUN groupadd -g 10001 -r nonroot && useradd -g 10001 -r -u 10001 nonroot

USER 10001:10001

CMD openETPServer server --start --log_level info --authN none --authZ delegate=$OSDU_HOST

