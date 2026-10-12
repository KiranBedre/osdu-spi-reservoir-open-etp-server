#!/usr/bin/env bash

set -euo pipefail

echo "Checking for missing packages"
if ! command -v cppcheck &> /dev/null
then
   echo "-- Installing cppcheck in this image"
   apt-get update &> /dev/null
   apt-get install -y cppcheck && echo -e "\tOK"
fi

if ! command -v cppcheck-codequality &> /dev/null
then
    echo "--Installing cppcheck-codequality in this image"
    apt-get update &> /dev/null
    apt-get install -y python3-pip &> /dev/null
    python3 -m pip install cppcheck-codequality && echo -e "\tOK"
fi

echo "Running cppcheck for static code analysis"

SOURCE_TREE=/source
BUILD_TREE="${SOURCE_TREE}/build"

cd "${BUILD_TREE}"

if [ ! -d cppcheck-files ]; then
    echo "--Creating Cppcheck directory"
    mkdir -p cppcheck-files && echo -e "\tOK"
fi

# Clean previous scan
rm -rf cppcheck-files/*

cppcheck \
    -j 8 \
    --project=${BUILD_TREE}/using_docker_debug/compile_commands.json \
    --cppcheck-build-dir=cppcheck-files \
    --xml-version=2 \
    --output-file=cppcheck-files/cppcheck_output.xml \
    --enable=all \
    --suppressions-list=${SOURCE_TREE}/devops/cppcheck/cppcheck_suppress_list.txt \
    --inline-suppr \
    --std=c++17 \
    --library=${SOURCE_TREE}/devops/cppcheck/cfg/boost.cfg \
    --relative-paths=${SOURCE_TREE}

echo "Finished creating xml, now creating the HTML Report"

cppcheck-htmlreport \
   --file=cppcheck-files/cppcheck_output.xml \
   --report-dir=cppcheck-files/cppcheck-html-report \
   --source-dir=${SOURCE_TREE} \
   --title=OPEN-ETP-SERVER &> /dev/null

echo "Converting XML to a Codequality JSON report"

cppcheck-codequality \
  --base-dir ${SOURCE_TREE} \
  --input-file cppcheck-files/cppcheck_output.xml \
  --output-file cppcheck-files/cppcheck_codequality.json