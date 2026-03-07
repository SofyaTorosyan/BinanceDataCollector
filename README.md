# BinanceDataCollector

## Build

### Conan install

```bash
conan install --build=missing
```

### CMake configure
```bash
cmake -S . --preset conan-default
```

### Build

```bash
cmake --build build --preset conan-release
```
