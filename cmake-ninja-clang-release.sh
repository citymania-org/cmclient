# Use ninja and clang for faster build time, use Release and no asserts for best performance
cmake .. -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DOPTION_USE_ASSERTS=OFF -DCMAKE_COLOR_DIAGNOSTICS=ON ..
