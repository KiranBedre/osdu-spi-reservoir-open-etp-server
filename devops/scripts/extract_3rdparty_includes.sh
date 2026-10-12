#!/usr/bin/env bash

set -euo pipefail

# cleanup previous copied includes
rm -rf build/extracted-3rdparty-includes

echo "Extracting all 3rd party include files from source code"

# find all #include<aaa> to detect all 3rd parties include`
#SRC_INCLUDES=$(find src -type f | xargs grep -irhoP "^#include <\K([^>]+)(?=>.*)" | sort | uniq | grep -v "^oes/" )
SRC_INCLUDES=$(find src -type f | xargs grep -irhoP "^#include <\K([^>]+)(?=>.*)" | sort | uniq  )

# Remove windows specific includes
SRC_INCLUDES=${SRC_INCLUDES//iso646\.h/}
SRC_INCLUDES=${SRC_INCLUDES//psapi\.h/}
SRC_INCLUDES=${SRC_INCLUDES//windows\.h/}
SRC_INCLUDES=${SRC_INCLUDES//winsock\.h/}

# Search for the files in VCPKG and system include
SEARCH_PATH="/source/build/using_docker_debug/vcpkg_installed/x64-linux/include /source/src/lib /usr/include/c++/11 /usr/include/x86_64-linux-gnu /usr/include"

for d in $SEARCH_PATH; do
  NOTFOUND=""
  for i in $SRC_INCLUDES; do
    if [[ -f $d/$i ]]; then
      mkdir -p `dirname build/extracted-3rdparty-includes$d/$i` 
      cp $d/$i build/extracted-3rdparty-includes$d/$i
    else
      NOTFOUND="$NOTFOUND $i"
    fi
  done
  SRC_INCLUDES=$NOTFOUND
done


# List all files not found if any
if [ -z "$NOTFOUND" ] 
then
  echo "All 3rd party dependencies were found"
else
  echo "Remaining unmatch includes: \"$NOTFOUND\""
fi