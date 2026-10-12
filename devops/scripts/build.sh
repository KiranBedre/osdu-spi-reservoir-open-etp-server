#!/usr/bin/env bash

set -euo pipefail

SKIP_CONFIGURE=false
SKIP_BUILD=false
SKIP_UNIT_TESTS=false
SKIP_E2E_TESTS=false
SKIP_CODE_COVERAGE=false
SKIP_PACKAGING=false

if [[ $# -gt 0 && "$1" == "--debug" ]]; then
    BUILD_TYPE=Debug
    SKIP_PACKAGING=true
    if [[ -z "${CODE_COVERAGE:-}" ]]; then
        export CODE_COVERAGE=ON
    elif [ ${CODE_COVERAGE} == "OFF" ]; then
        SKIP_CODE_COVERAGE=true
    fi
    # added DCMAKE_EXPORT_COMPILE_COMMANDS=ON for creating compile_commands.json inside /build/using_docker_debug
    # this file will be used for cppcheck later from cppcheck_scan.sh script
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCODE_COVERAGE=${CODE_COVERAGE} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    # remove the parameter
    shift
else
    BUILD_TYPE=Release
    SKIP_CODE_COVERAGE=true
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
fi

# Add VCPKG toolchain
echo -e "Add VCPKG toolchain: ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
CMAKE_OPTIONS+=" -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"

if [[ $# -lt 1 ]]; then
    echo -e "$0 [--debug] BUILD_DIR [ninja_args]..."
    echo -e "\--debug\n\t\tPerform Debug build (Default: Release build)"
    echo -e "\tBUILD_DIR\n\t\tPath to the build directory to use"
    echo -e "\tninja_args\n\t\tAny argument that will be forwarded to ninja"
    exit 1
fi

# Coloration codes
Black='\033[0;30m'
DarkGray='\033[1;30m'
Red='\033[0;31m'
LightRed='\033[1;31m'
Green='\033[0;32m'
LightGreen='\033[1;32m'
Orange='\033[0;33m'
Yellow='\033[1;33m'
Blue='\033[0;34m'
LightBlue='\033[1;34m'
Purple='\033[0;35m'
LightPurple='\033[1;35m'
Cyan='\033[0;36m'
LightCyan='\033[1;36m'
LightGray='\033[0;37m'
White='\033[1;37m'
NC='\033[0m' # No Color

# Adding error reporting
err_report() {
    echo ""
    echo -e "${Red}Error on $0:$1${NC}: stop execution."
}
trap 'err_report $LINENO' ERR


# Actual build
SOURCE_TREE="${PWD}"
BUILD_TREE="${SOURCE_TREE}/${1}"

# Removing first mandatory parameter
shift

# To know the global execution time:
main_start=`date +%s`

if [ ! -d "$BUILD_TREE" ]; then
    echo "Creating build directory"
    mkdir -p "${BUILD_TREE}" && echo -e "\tOK"
fi

cd "${BUILD_TREE}"


echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightBlue}Cleanup UnitTest directory${NC}"
rm -rf tests

echo -e "${LightBlue}Cleanup Install directory${NC}"
rm -rf dist

echo -e "${LightBlue}Cleanup CodeCoverage directory${NC}"
rm -rf gcovr

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"

# Configure build
if [ "${SKIP_CONFIGURE}" = true ]; then
    echo -e "${LightBlue}Skipping Configure${NC}"
else
    echo -e "${LightBlue}Configuring build tree using CMake and Ninja for ${BUILD_TYPE} build${NC}"
    echo -e "Running (in ${Green}$PWD${NC}): ${Yellow}cmake -GNinja ${CMAKE_OPTIONS} \"${SOURCE_TREE}\" -DCMAKE_INSTALL_PREFIX=\"./dist\"${NC}"
    start=`date +%s`
    cmake -GNinja ${CMAKE_OPTIONS} "${SOURCE_TREE}" -DCMAKE_INSTALL_PREFIX="./dist"
    end=`date +%s`
    echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
fi

# Actual build
echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
if [ "${SKIP_BUILD}" = true ]; then
    echo -e "${LightBlue}Skipping Build${NC}"
else
    echo -e "${LightBlue}Building${NC}"
    echo -e "Running (in ${Green}$PWD${NC}): ${Yellow}ninja $*${NC}"
    start=`date +%s`
    ninja "$@"
    end=`date +%s`
    echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
fi

# Remove local temporary file
rm -f a.out

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
if [ "${SKIP_UNIT_TESTS}" = true ]; then
    echo -e "${LightBlue}Skipping UnitTests${NC}"
else
    echo -e "${LightBlue}Running UnitTests:${NC}"
    echo -e "Running (in ${Green}$PWD${NC}) (log written in ${Cyan}${PWD}/Testing/Temporary/LastTest.log${NC}): ${Yellow}ctest . -v${NC}"
    start=`date +%s`
    # Testing will only run sequentially (testing multi-partition support on one database)
    if (ctest . -v); then
        end=`date +%s`
        echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
    else
        echo -e "\t${Red}FAILED with return code: $?${NC}"
        echo -e "${Purple}<<<<< logging file <<<<<${NC}"
        cat ./Testing/Temporary/LastTest.log
        echo -e "${Purple}>>>>> logging file >>>>>${NC}"
        echo -e "You can read this content in file: ${Yellow}${PWD}/Testing/Temporary/LastTest.log${NC}"
        exit 1
    fi
fi

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
if [ "${SKIP_E2E_TESTS}" = true ]; then
    echo -e "${LightBlue}Skipping End to End tests${NC}"
else
    echo -e "${LightBlue}Running End to End tests:${NC}"
    echo -e "Running (in ${Green}$PWD${NC}): ${Yellow}../../tests/run_e2e_test.sh${NC}"
    start=`date +%s`
    E2E_OPTION=""
    if [ ${BUILD_TYPE} == "Debug" ]; then
        E2E_OPTION="--debug"
    fi
    if ( bash ../../tests/run_e2e_test.sh ${E2E_OPTION}); then
        end=`date +%s`
        echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
    else
        echo -e "\t${Red}FAILED with return code: $?${NC}"
        exit 1
    fi
fi


echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
if [ "${SKIP_CODE_COVERAGE}" = true ]; then
    echo -e "${LightBlue}Skipping Code Coverage${NC}"
    # Cleanup previous GCOV files if exists
    find "${SOURCE_TREE}" -type f \( -name '*.gcov' -o -name '*.gcda' \) -exec rm -r {} \;
else
    echo -e "${LightBlue}Running Code Coverage:${NC}"
    GCOV_FILTER_OPTIONS="-r ../../src"
    GCOV_EXCLUDE_OPTIONS="-e ../../src/benchmark/ -e ../../src/bin/osduStubServer/ -e ../../src/lib/oes/testing/ -e ../../src/tests/"
    GCOV_OUTPUT_OPTIONS="--html gcovr/html/index.html --html-details --xml gcovr/cobertura/openetpserver.xml"
    GCOV_OPTIONS="-j 8 -s --delete --gcov-ignore-parse-errors ${GCOV_FILTER_OPTIONS} ${GCOV_EXCLUDE_OPTIONS} ${GCOV_OUTPUT_OPTIONS}"
    echo -e "Running (in ${Green}$PWD${NC}): ${Yellow}gcovr ${GCOV_OPTIONS} . ${NC}"
    start=`date +%s`
    mkdir -p gcovr/html
    mkdir -p gcovr/cobertura

    if (gcovr ${GCOV_OPTIONS} .); then
        end=`date +%s`
        echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
    else
        echo -e "\t${Red}FAILED with return code: $?${NC}"
        # Cleanup GCOV files if not done by gcov
        find "${SOURCE_TREE}" -name *.gcov -exec rm -r {} \;
        exit 1
    fi
fi

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
if [ "${SKIP_PACKAGING}" = true ]; then
    echo -e "${LightBlue}Skipping Packaging${NC}"
else
    echo -e "${LightBlue}Packaging:${NC}"
    filename="openETPServer"
    echo -e "Running (in ${Green}$PWD${NC}) (log written in ${Cyan}${filename}.log${NC}): ${Yellow}ninja install${NC}"
    start=`date +%s`
    ninja install
    end=`date +%s`
    echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"

    echo -e "\n${LightBlue}Checking dist program can start:${NC}"
    echo -e "Running (in ${Green}$PWD${NC}) (log written in ${Cyan}${PWD}/start_dist.log${NC}): ${Yellow}./dist/bin/openETPServer -h${NC}"
    start=`date +%s`
    if (./dist/bin/openETPServer -h > start_dist.log 2>&1); then
        end=`date +%s`
        echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
    else
        echo -e "\t${Red}FAILED with return code: $?${NC}"
        echo -e "${Purple}<<<<< logging file <<<<<${NC}"
        cat ./start_dist.log
        echo -e "${Purple}>>>>> logging file >>>>>${NC}"
        echo -e "You can read this content in file: ${Yellow}${PWD}/Testing/Temporary/LastTest.log${NC}"
        exit 1
    fi

    echo -e "\n${LightBlue}Compressing dist program:${NC}"
    cd dist
    echo -e "Running (in ${Green}$PWD${NC}) (log written in ${Cyan}${PWD}/../compress_dist.log${NC}): ${Yellow}tar -czvf ../dist.tgz .${NC}"
    start=`date +%s`
    if (tar -czvf ../dist.tgz .> ../compress_dist.log 2>&1); then
        end=`date +%s`
        echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
    else
        echo -e "\t${Red}FAILED with return code: $?${NC}"
        echo -e "${Purple}<<<<< logging file <<<<<${NC}"
        cat ../compress_dist.log
        echo -e "${Purple}>>>>> logging file >>>>>${NC}"
        echo -e "You can read this content in file: ${Yellow}${PWD}/Testing/Temporary/LastTest.log${NC}"
        exit 1
    fi
    cd ..
fi

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
end=`date +%s`
echo -e "Build and tests are ${Green}OK (`expr $end - $main_start`s)${NC}"
