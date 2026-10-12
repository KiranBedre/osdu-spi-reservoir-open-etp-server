#!/usr/bin/env bash

# ============================================================================
# Copyright 2016-2021 Emerson Paradigm Holding LLC. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

dataspace_options=""
epc_result_dataset=""
h5_result_dataset=""
log_filename=""
oes_exe=""
oes_pid=""
function cleanup {
  echo -e "${LightCyan}Cleaning test environment${NC}"
  if [ "${dataspace_options}" != ""  ]; then
    ./${oes_exe} space ${dataspace_options} --delete
  fi
  if [ ${oes_pid} ]; then
    kill ${oes_pid}
  fi
  rm -f  ${h5_result_dataset} ${epc_result_dataset} ${log_filename}
  rm -rf results
}
trap cleanup EXIT

set -euo pipefail

if [[ $# -gt 0 && "$1" == "--debug" ]]; then
    BUILD_TYPE=Debug
    # remove the parameter
    shift
else
    BUILD_TYPE=Release
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

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
oes_exe=openETPServer
log_filename=E2E_openETPServer.log

# Use simple log format without time stamp.
export LOG_FORMAT="%P: %M"

echo -e "${LightBlue}Testing that ${oes_exe} executable exists${NC}"
if [[ -f ${oes_exe} && -x ${oes_exe} ]]; then
    echo -e "\t${Green}OK${NC}"
else
    echo -e "\t${Red}FAILED${NC}"
    exit 1
fi

function check_test {
    rm -rf results
    mkdir -p ./results/
    log_file=${BUILD_TYPE}"_$(echo -e "$*" | tr -d '[:space:]' | tr -d '[/.:&]')".log
    # Trim non ascii characters and date
    cat ${log_filename} |
      tr -cd '\001-\177' |
      tr -d '\015' | # taking care of the Windows Line Ending
      sed 's/\[::ffff:127.0.0.1\]:[0-9]* /[::ffff:127.0.0.1\]:00000 /g' | # Cleanup socket
      sed '/\[connect\]/d' | # Cleanup connect server messages
      sed '/\[disconnect\]/d' | # Cleanup disconnect server messages
      sed 's/host=host.docker.internal/host=open-etp-postgresql/g' | # Cleaning Postgresql host connection
      sed 's/Created: .*/Created: Mon Jan 00 00:00:00 0000/g' | # Cleanup date
      sed 's/Last-Modified: .*/Last-Modified: Mon Jan 00 00:00:00 0000/g' |
      sed 's/\[[0-9]\{4\}-[0-1][0-9]-[0-3][0-9]\s[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\]/\[0000-00-00 00:00:00\]/g' |
      sed 's/"applicationVersion": "OSDU M26 - .*",/"applicationVersion": "OSDU M26",/g' | # Cleanup Server version
      sed 's/ETP-1.2 Server [(]OSDU M26 - .*[)]/ETP-1.2 Server (OSDU M26)/g' |
      sed 's/\[[0-9]\{4\}-[a-zA-Z]\{3\}-[0-3][0-9]\s[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\]//g' | # date with alphabetical month
      sed 's/[-]\{0,1\}[0-9]\{1,\}\.[0-9]\{1,\}s/sometime_in_seconds/g' | # time
      sed 's/[0-9]\{1,\}\.[0-9]\{1,\}\ MiB/some_number\ MiB/g' | # MiB and below MB
      sed 's/[0-9]\{1,\}\ MB\ peak/some_number\ MB\ peak/g' > ./results/${log_file}

    if [ ${BUILD_TYPE} == "Debug" ]; then
      echo ${log_file}
      echo
      echo "begin"
      cat ./results/${log_file}
      echo "end"
    fi

    if [ -f  ../../tests/ref/${log_file} ]; then
      diff -w --suppress-blank-empty --strip-trailing-cr ./results/${log_file} ../../tests/ref/${log_file}
    else
      cp ./results/${log_file} ../../tests/ref
    fi
}
function run_test_no_check {
        echo -e "Running (in ${Green}$PWD${NC}) (log written in ${Cyan}${log_filename}${NC}): ${Yellow}$*${NC}"
        start=`date +%s`
        if ( $* > ./${log_filename} 2>&1); then
            end=`date +%s`
            echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
        else
            echo -e "\t${Red}FAILED with return code: $?${NC}"
            echo -e "${Purple}<<<<< STDOUT <<<<<${NC}"
            cat ./${log_filename}
            echo -e "${Purple}>>>>> STDOUT >>>>>${NC}"
            exit 1
        fi
}
function run_test {
    run_test_no_check $*
    check_test $*
}
function run_expected_error_test {
        echo -e "Running (in ${Green}$PWD${NC}) (log written in ${Cyan}${log_filename}${NC}): ${Yellow}$*${NC}"
        start=`date +%s`
        if ( $* > ./${log_filename} 2>&1); then
            echo -e "\t${Red}FAILED with return code: $?${NC}"
            echo -e "${Purple}<<<<< STDOUT <<<<<${NC}"
            cat ./${log_filename}
            echo -e "${Purple}>>>>> STDOUT >>>>>${NC}"
            exit 1
        else
            end=`date +%s`
            echo -e "\t${Green}OK (`expr $end - $start`s)${NC}"
        fi
}


echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightCyan}Go through each openETPServer commands${NC}"
run_expected_error_test ./${oes_exe} unkwown_cmd --help
run_test ./${oes_exe} hdf5 --help
run_test ./${oes_exe} server --help
run_test ./${oes_exe} probe --help
run_test ./${oes_exe} space --help

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightCyan}Starting a local server${NC}"
server_credentials="--authN none  --port 9002"
server_start_option="${server_credentials} -M 2GB --log_level=warning"
if [ ${BUILD_TYPE} == "Debug" ]; then
  server_credentials="--authN none  --port 9003"
  server_start_option="${server_credentials} -M 2GB --nobps --debug-main --log_level=warning"
fi

run_test ./${oes_exe} server ${server_start_option} --start &
# Capture openETPServer PID to do proper cleaning at exit
oes_pid=$!

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bash "${repo_root}/devops/scripts/fetch-test-fixtures.sh" e2e "${repo_root}/data"
h5_demo_dataset="${repo_root}/data/Volve_Demo_Reservoir_Grid_Depth.h5"
epc_demo_dataset="${repo_root}/data/Volve_Demo_Reservoir_Grid_Depth.epc"
h5_result_dataset=./E2E_Test_${BUILD_TYPE}.h5
epc_result_dataset=./E2E_Test_${BUILD_TYPE}.epc

sleep 10

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightCyan}ETP server command tests${NC}"
run_test ./${oes_exe} server ${server_credentials} --init
run_test ./${oes_exe} server ${server_credentials} --check
run_test ./${oes_exe} server ${server_credentials} --version
run_expected_error_test  ./${oes_exe} server ${server_credentials} --unkwown_option

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightCyan}ETP probe command tests${NC}"
server_con="-S ws://localhost:9002 -A none"
if [ ${BUILD_TYPE} == "Debug" ]; then
  server_con="-S ws://localhost:9003 -A none"
fi
run_expected_error_test ./${oes_exe} probe ${server_con} --unkwown_option
run_test_no_check ./${oes_exe} probe ${server_con} --ping
run_test ./${oes_exe} probe ${server_con} --info
run_expected_error_test ./${oes_exe} probe ${server_con} --osdu

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightCyan}ETP dataspace command tests${NC}"
dataspace_options="${server_con} --log_level=warning -s E2E/Test_${BUILD_TYPE}"
run_expected_error_test ./${oes_exe} space ${dataspace_options} --unkwown_option
run_test ./${oes_exe} space ${server_con} --list --log_level=warning
run_test ./${oes_exe} space ${dataspace_options} --new
run_expected_error_test ./${oes_exe} space ${dataspace_options} --new
run_test ./${oes_exe} space ${dataspace_options} --toc
run_test ./${oes_exe} space ${dataspace_options} --info
run_test ./${oes_exe} space ${dataspace_options} --import-epc ${epc_demo_dataset}
run_test ./${oes_exe} space ${dataspace_options} --export-epc ${epc_result_dataset} --overwrite 
run_test ./${oes_exe} space ${dataspace_options} --delete
run_expected_error_test ./${oes_exe} space ${dataspace_options} --delete
dataspace_options=""

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
echo -e "${LightCyan}ETP hdf5 command tests${NC}"
run_expected_error_test ./${oes_exe} hdf5 --unkwown_option
run_test ./${oes_exe} hdf5 --info  ${h5_result_dataset}
run_test ./${oes_exe} hdf5 --stats ${h5_result_dataset}
run_test ./${oes_exe} hdf5 --diff  ${h5_result_dataset} --with ${h5_demo_dataset}

echo -e "${LightCyan}-------------------------------------------------------------------------${NC}"
if [ "${oes_pid}" != "" ]; then
  kill ${oes_pid}
  oes_pid=""
fi
echo -e "${LightCyan}DONE${NC}"
