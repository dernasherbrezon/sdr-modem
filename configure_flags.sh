#!/bin/bash

CPU=$1

if [ "${CPU}" = "arm1176jzf-s" ]; then
   export CXXFLAGS="-mcpu=${CPU} -mfpu=vfp -mfloat-abi=hard"
elif [ "${CPU}" = "cortex-a53" ]; then
   export CXXFLAGS="-mcpu=${CPU} -mfpu=neon-fp-armv8 -mfloat-abi=hard"
elif [ "${CPU}" = "cortex-a7" ]; then
   export CXXFLAGS="-mcpu=${CPU} -mfpu=neon-vfpv4 -mfloat-abi=hard"
elif [ "${CPU}" = "cortex-a72" ]; then
   export CXXFLAGS="-mcpu=${CPU} -mfpu=neon-fp-armv8 -mfloat-abi=hard"
elif [ "${CPU}" = "generic" ]; then
   export CXXFLAGS=""
else
   echo "unknown core: ${CPU}"
   exit 1
fi

if [[ "${CPU}" = "generic" ]]; then
   export BUCKET=r2cloud
else
   export BUCKET="r2cloud/cpu-${CPU}"
fi

export ASMFLAGS="${CXXFLAGS} -mthumb -g"
export CFLAGS=${CXXFLAGS}

echo "CFLAGS   - ${CFLAGS}"
echo "ASMFLAGS - ${ASMFLAGS}"
echo "CXXFLAGS - ${CXXFLAGS}"

echo "CFLAGS=${CFLAGS}" >> $GITHUB_ENV
echo "ASMFLAGS=${ASMFLAGS}" >> $GITHUB_ENV
echo "CXXFLAGS=${CXXFLAGS}" >> $GITHUB_ENV
