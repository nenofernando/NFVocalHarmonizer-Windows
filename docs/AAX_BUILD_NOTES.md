# AAX build notes — NF Vocal Harmonizer

SDK usado (mesmo do Vocal Verb):

```text
/Users/nenofernando/Downloads/aax-sdk-2-9-0
```

Configure + build:

```bash
cmake -S . -B build-aax \
  -DJUCE_DIR=/Users/nenofernando/Downloads/JUCE \
  -DNF_ENABLE_AAX=ON \
  -DJUCE_AAX_SDK_PATH=/Users/nenofernando/Downloads/aax-sdk-2-9-0 \
  -DNF_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

cmake --build build-aax --config Release --parallel
```

Artefato:

```text
build-aax/NFVocalHarmonizer_artefacts/Release/AAX/NF Vocal Harmonizer.aaxplugin
```

Notas:

- `juce_set_aax_sdk_path` + `AAX_CATEGORY` já estão no `CMakeLists.txt`.
- IDs de parâmetro não mudam ao ligar AAX.
- Assinatura PACE/iLok e instalação em `/Library/Application Support/Avid/Audio/Plug-Ins` ficam fora desta fase.
