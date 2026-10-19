#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import pathlib
import re
import os


ignored_files_re = re.compile(r"(CMakeLists.txt|files.cmake)$")
files_cmake_re = re.compile(r"(.*\.(?:h|hpp|hh|c|cc|cpp))$", re.IGNORECASE)


def handle_files_cmake(files_cmake):
    print(f"handling {files_cmake}")
    files_cmake_dir = files_cmake.parent
    used_files = set()
    with open(files_cmake, "rt") as fd:
        for line in fd:
            # cleanup line from comments
            line = line.split("#")[0].strip()
            if m:=files_cmake_re.match(line):  # noqa: E231 E701
                used_files.add(files_cmake_dir / m.group(1))
    # list all files in dir and subdirs:
    full_list = set()
    for f in files_cmake_dir.rglob("*"):
        if f.is_dir():
            continue
        elif ignored_files_re.match(str(f.name)):
            continue
        elif files_cmake_re.match(str(f)):
            full_list.add(f)
        else:
            # Uncomment next line for detecting unhandeled files
            # print("  ", f)
            pass
    unused_files = full_list - used_files
    if unused_files:
        print("   Files CMake is not aware of:")
        for f in sorted(unused_files):
            print(f"     {f.relative_to(os.getcwd())}")
        print(f"   Files used: {len(used_files)}/{len(full_list)}")
    else:
        print(f"   All {len(full_list)} files are well used")
        return True
    return False


def main():
    root_dir = pathlib.Path(__file__).parent.absolute()
    print(root_dir)
    result = True
    for item in sorted(root_dir.rglob("files.cmake")):
        result = handle_files_cmake(item) and result
    return 0


if __name__ == '__main__':
    exit(main())
