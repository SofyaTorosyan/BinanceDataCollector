# BinanceDataCollector

## Build

### Conan install

```bash
conan install --build=missing
```

### CMake configure

#### Linux
```bash
cmake -S . --preset conan-release
```

### Build

```bash
cmake --build build --preset conan-release
```
