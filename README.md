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

## Deploy (Linux — systemd)

Create a dedicated user and install the binary and config:

```bash
sudo useradd -r -s /sbin/nologin bdc
sudo mkdir -p /opt/bdc
sudo cp build/Release/src/app/BinanceDataCollector /opt/bdc/
sudo cp config.json /opt/bdc/
sudo chown -R bdc:bdc /opt/bdc
```

Install and enable the service:

```bash
sudo cp deploy/linux/binancedatacollector.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now binancedatacollector
```

Check status and logs:

```bash
sudo systemctl status binancedatacollector
journalctl -u binancedatacollector -f
```

Stop / restart:

```bash
sudo systemctl stop binancedatacollector
sudo systemctl restart binancedatacollector
```
