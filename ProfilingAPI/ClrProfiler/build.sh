#!/bin/sh

[ -z "${CORECLR_PATH:-}" ] && CORECLR_PATH=~/coreclr
[ -z "${BuildOS:-}"      ] && BuildOS=Linux
[ -z "${BuildArch:-}"    ] && BuildArch=x64
[ -z "${BuildType:-}"    ] && BuildType=Debug
[ -z "${Output:-}"       ] && Output=CorProfiler.so

printf '  CORECLR_PATH : %s\n' "$CORECLR_PATH"
printf '  BuildOS      : %s\n' "$BuildOS"
printf '  BuildArch    : %s\n' "$BuildArch"
printf '  BuildType    : %s\n' "$BuildType"

printf '  Building %s ... ' "$Output"

CXX_FLAGS="$CXX_FLAGS -Wno-invalid-noreturn -fPIC -fms-extensions -DBIT64 -DPAL_STDCPP_COMPAT -DPLATFORM_UNIX -std=c++11"
INCLUDES="-I $CORECLR_PATH/src/pal/inc/rt -I $CORECLR_PATH/src/pal/prebuilt/inc -I $CORECLR_PATH/src/pal/inc -I $CORECLR_PATH/src/inc -I $CORECLR_PATH/bin/Product/$BuildOS.$BuildArch.$BuildType/inc"

mkdir -p obj/$BuildType/$BuildArch

clang++-3.9 -shared -o obj/$BuildType/$BuildArch/$Output $CXX_FLAGS $INCLUDES ClassFactory.cpp dllmain.cpp

printf 'Done.\n'
