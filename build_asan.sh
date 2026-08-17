mkdir -p build-asan && cd build-asan && cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  -S .. -B . 2>&1

for file in test_*; do
  ASAN_OPTIONS=detect_leaks=1 ./${file}
done