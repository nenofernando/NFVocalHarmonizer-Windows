param([Parameter(Mandatory=$true)][string]$JuceDir)
$ErrorActionPreference = "Stop"
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DJUCE_DIR="$JuceDir" -DNF_BUILD_TESTS=ON
cmake --build build-windows --config Release
ctest --test-dir build-windows -C Release --output-on-failure

