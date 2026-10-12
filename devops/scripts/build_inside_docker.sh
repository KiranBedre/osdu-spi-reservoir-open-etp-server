#!/usr/bin/env bash

set -euo pipefail

USER_NAME=running_user
# Create group and user identically to running user.
if [ ! "$(getent group running_user_group)" ]; then
    echo "Creating running user group"
    SOURCE_TREE_GID=$(stat -c '%g' .)
    if [ "$SOURCE_TREE_GID" -eq "0" ]; then
        USER_NAME=root
    else
        groupadd -r running_user_group -g "${SOURCE_TREE_GID}" && echo -e "\tOK"
    fi
fi
if [ ! "$(getent passwd running_user)" ]; then
    echo "Creating running user"
    SOURCE_TREE_UID=$(stat -c '%u' .)
    if [ "$SOURCE_TREE_UID" -eq "0" ]; then
        USER_NAME=root
    else
        useradd --no-log-init -r -u "${SOURCE_TREE_UID}" -g running_user_group running_user && echo -e "\tOK"
    fi
fi


# Configure VCPKG binary cache.
if [ "${VCPKG_DEFAULT_BINARY_CACHE}" = "" ]; then
    export VCPKG_DEFAULT_BINARY_CACHE="$PWD/.vcpkg_cache/"
    if [ -d "$VCPKG_DEFAULT_BINARY_CACHE" ]; then
        echo "Use existing VCPKG binary cache: ${VCPKG_DEFAULT_BINARY_CACHE}"
    else
        echo "Create directory for VCPKG binary cache: ${VCPKG_DEFAULT_BINARY_CACHE}"
        mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
    fi
    chmod 777 -R ${VCPKG_DEFAULT_BINARY_CACHE}
else
    echo "Use shared VCPKG binary cache from host machine: ${VCPKG_DEFAULT_BINARY_CACHE}";
fi


echo "#########################################################################"
figlet -c "Debug"
su -m $USER_NAME -c "bash devops/scripts/build.sh --debug build/using_docker_debug"

echo "#########################################################################"
figlet -c "Release"
su -m $USER_NAME -c "bash devops/scripts/build.sh build/using_docker"

echo "#########################################################################"
echo "Copy third party includes for FOSSA analysis"
su -m $USER_NAME -c "bash devops/scripts/extract_3rdparty_includes.sh"
