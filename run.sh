#!/bin/bash

print_helper() {
    echo " "
    echo "run.sh [options] command (arg)"
    echo "command the command you want to run. Can be:"
    echo "  u, up or build for build the image (arg) (or local build)"
    echo "  rm or remove to remove the image (arg)"
    echo "  r or run to start the container with bash for a given image (arg)"
    echo "--run runs the container bash after a build/up command (used for debugging)"
    echo "--rm deletes the last image before a build/up command"
    echo "--local indicates to build the etp-server locally (OS native)"
    echo "arg is the image (build, runtime). It is ignored if --local flag is passed"
}


local_build() {
    sudo apt-get install -y curl zip unzip tar cmake g++ pkg-config bison flex autoconf ninja-build
    which_so="unknown"
    case "$(uname -sr)" in
        Linux*)
            which_so="linux"
            ;;
        CYGWIN*|MINGW*|MINGW32*|MSYS*)
            which_so="windows"
            ;;
        *)
            echo "Unsupported SO."
            exit 1
            ;;
    esac

    if [ $which_so == "linux" ]; then
        cmake -GNinja -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug
    else
        cmake -GNinja -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug
    fi
    cmake --build build

}

WHICH_IMAGE="debug" # can be runtime
check_which_image() {
    if [[ $# -lt 1 ]]; then
        echo "[check_which_image] Error: no arguments were provided."
    elif [ "$1" == "build" ]; then
        WHICH_IMAGE="build"
    elif [ "$1" == "runtime" ]; then
        WHICH_IMAGE="runtime"
    else
        echo "Invalid option: $1"
        exit 1
    fi


}

docker_remove_image() {
    case $WHICH_IMAGE in
        "debug")
            echo "The build image may take a lot of time to be built again."
            read -p "Are you sure you want to remove the build image? (y/n)" yn
            case $yn in
                [Yy]* )
                    docker image rm -f open_etp_server_build
                    ;;
                [Nn]* )
                    exit 0
                    ;;
                *)
                    echo "Not valid answer."
                    exit 1
                    ;;
            esac
            ;;
        "runtime")
            docker image rm -f open_etp_server_runtime
            ;;
    esac
}

docker_compose_up() {
    case $WHICH_IMAGE in
        "build")
            docker compose up open_etp_server_build
            ;;
        "runtime")
            docker_remove_image
            docker compose up open_etp_server_runtime
            ;;
    esac
}

docker_compose_run() {

    case $WHICH_IMAGE in
        "build")
            docker compose run open_etp_server_build bash
            ;;
        "runtime")
            docker compose run open_etp_server_runtime bash
            ;;
    esac
}

COMMAND="blank"
RM_OPTION=false
RUN_OPTION=false
LOCAL_BUILD_OPTION=false

set_command() {
    if [[ "$#" -gt 0 && $COMMAND != "blank" ]]; then
        echo "Can not have multiple commands."
        exit 1
    fi

    COMMAND=$1
}


while test $# -gt 0; do
    case "$1" in
        -h|--help)
            print_helper
            exit 0
            ;;
        u|up)
            shift
            set_command "up"
            ;;
        r|run)
            shift
            set_command "run"
            ;;
        rm|remove)
            shift
            set_command "rm"
            ;;
        --rm)
            shift
            RM_OPTION=true
            ;;
        --run)
            shift
            RUN_OPTION=true
            ;;
        --local)
            shift
            LOCAL_BUILD_OPTION=true
            ;;
        *)
            if [ $# -gt 1 ]; then
                echo "Too many args passed: $@"
                exit 1
            fi
            check_which_image $1
            shift
            ;;
    esac
done



if [[ $COMMAND == "rm" || $RM_OPTION == true ]]; then
    docker_remove_image
fi

if [ $COMMAND == "up" ]; then
    if [ $LOCAL_BUILD_OPTION == true ]; then
        local_build
        exit 0
    else
        docker_compose_up
    fi
fi

if [[ $COMMAND == "run" || $RUN_OPTION == true ]]; then
    docker_compose_run
fi
