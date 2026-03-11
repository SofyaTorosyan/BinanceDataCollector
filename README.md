# BinanceDataCollector

A C++20 service that streams real-time trade events from the Binance WebSocket API, aggregates them into fixed-width time windows per trading pair, and writes statistics to disk.

---

## Table of Contents

- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Design Patterns & Rationale](#design-patterns--rationale)
- [Data Flow](#data-flow)
- [Configuration](#configuration)
- [Usage](#usage)
- [Build](#build)
- [Testing & Code Coverage](#testing--code-coverage)
- [Deploy (Linux — systemd)](#deploy-linux--systemd)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                          App                                │
│  (Boost.DI composition root — wires all dependencies)       │
└────────────────────────┬────────────────────────────────────┘
                         │ owns
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                   MonitoringService                         │
│  Registers WebSocket callbacks, drives flush timer          │
└──────┬─────────────────┬──────────────────┬─────────────────┘
       │ uses            │ uses             │ uses
       ▼                 ▼                  ▼
┌────────────┐  ┌────────────────┐  ┌──────────────────┐
│ WebSocket  │  │ TradeAggregator│  │  FileSerializer  │
│  Client    │  │ (time windows) │  │  (append output) │
│ (TLS/Beast)│  │  thread-safe   │  │                  │
└────────────┘  └────────────────┘  └──────────────────┘
       │                │
       │ raw JSON       │ WindowStats
       └───────────────►│
            TradeEvent  │
                        ▼
               Fixed-width windows
               keyed by (symbol, windowStartMs)
```

### Module overview

| Module | Library | Responsibility |
|--------|---------|----------------|
| `src/config` | `lib-config` | Load and validate `config.json` |
| `src/network` | `lib-network` | TLS WebSocket client with reconnection |
| `src/logging` | `lib-logging` | spdlog-backed structured logger |
| `src/market` | `lib-market` | Trade aggregation and monitoring orchestration |
| `src/serialization` | `lib-serialization` | Write window stats to file |
| `src/app` | executable | CLI arg parsing, DI wiring, signal handling |

---

## Project Structure

```
BinanceDataCollector/
├── src/
│   ├── config/
│   │   ├── AppConfig.h               # Plain config data struct
│   │   ├── IConfigReader.h           # Interface: getConfig()
│   │   └── JsonConfigReader.h/cpp    # Loads & validates config.json
│   ├── network/
│   │   ├── IWebSocketClient.h        # Interface: connect/disconnect/handlers
│   │   └── BinanceWebSocketClient.h/cpp  # Boost.Beast TLS client
│   ├── logging/
│   │   ├── LogLevel.h                # Enum + fromString()
│   │   ├── ILogger.h                 # Interface + template helpers
│   │   └── SpdlogLogger.h/cpp        # spdlog implementation
│   ├── serialization/
│   │   ├── WindowStats.h             # Aggregated window data struct
│   │   ├── ISerializer.h             # Interface: write(windows)
│   │   └── FileSerializer.h/cpp      # Append-mode file writer
│   ├── market/
│   │   ├── TradeEvent.h              # Raw trade data struct
│   │   ├── IAggregator.h             # Interface: addTrade / popWindows
│   │   ├── TradeAggregator.h/cpp     # Fixed-window bucketing (thread-safe)
│   │   ├── IMonitoringService.h      # Interface: start/stopMonitoring
│   │   └── MonitoringService.h/cpp   # Wires WebSocket → Aggregator → Serializer
│   └── app/
│       ├── ArgParser.h               # Generic CLI arg parser (header-only)
│       ├── App.h/cpp                 # Composition root (Boost.DI)
│       └── main.cpp                  # Entry point
├── tests/
│   ├── app/ArgParserTests.cpp
│   ├── config/ConfigLoaderTests.cpp
│   ├── logging/LoggerTests.cpp
│   ├── market/MonitoringServiceTests.cpp
│   ├── market/TradeAggregatorTests.cpp
│   ├── network/BinanceWebSocketClientTests.cpp
│   ├── network/BinanceWebSocketClientIntegrationTests.cpp
│   └── serialization/FileSerializerTests.cpp
├── deploy/
│   └── linux/
│       └── binancedatacollector.service   # systemd unit
├── cmake/
│   └── Coverage.cmake                # lcov/genhtml helpers
└── CMakeLists.txt
```

---

## Design Patterns & Rationale

### Interface segregation + dependency injection

Every module exposes a pure abstract interface (`IConfigReader`, `IWebSocketClient`, `ILogger`, `IAggregator`, `ISerializer`, `IMonitoringService`). Concrete implementations are injected at the composition root via **Boost.DI**.

**Why:** Consumers depend only on the interface they need. Each module can be tested in isolation by substituting a mock. Adding an alternative implementation (e.g., a different exchange client) requires no changes to dependent code.

### Separation of concerns

| Layer | What it knows | What it does NOT know |
|-------|---------------|----------------------|
| `BinanceWebSocketClient` | TCP/TLS/WebSocket | Trade parsing, business logic |
| `MonitoringService` | Callback wiring, flush scheduling | JSON protocol details |
| `TradeAggregator` | Time-window math | Network, serialization |
| `FileSerializer` | File I/O | Trade logic, windowing |

**Why:** Each class has a single axis of change. A protocol change only touches the network layer; a storage change only touches the serializer.

### Strategy pattern (serializer)

`ISerializer` lets the storage backend be swapped without modifying `MonitoringService`. A database writer, Kafka producer, or in-memory store would just implement the same interface.

### RAII & smart pointers

All heap-allocated objects are managed via `std::shared_ptr`. Background threads are joined in destructors. The WebSocket io_context is protected by a work guard that is destroyed before joining the thread.

### Thread model

```
Main thread
  └─► keepRunningUntilSignal() — blocks on signal io_context

WebSocket thread (spawned by BinanceWebSocketClient)
  └─► io_context.run() — handles all async TCP/TLS/WS I/O
      All operations serialized through a strand.

Timer thread (spawned by MonitoringService)
  └─► timer io_context — fires every serializationIntervalMs
      Calls aggregator->popCompletedWindows() then serializer->write()
```

`TradeAggregator` is the only shared mutable state between the WebSocket thread (writes) and the timer thread (reads). It is protected by a mutex.

### Exponential back-off reconnection

`BinanceWebSocketClient` reconnects automatically on unexpected disconnection. The delay starts at 1 s and doubles on each failure up to `reconnectMaxDelayMs`. A deliberate `disconnect()` sets an `m_intentionalDisconnect` flag, suppressing reconnection.

**Why:** Avoids hammering the server during outages while recovering quickly from transient failures.

---

## Data Flow

```
Binance WebSocket API
        │  JSON aggTrade messages over TLS
        ▼
BinanceWebSocketClient::onRead()
        │  raw JSON string
        ▼
MonitoringService::onMessage()
        │  parseMessage() → TradeEvent{symbol, price, qty, time, side}
        ▼
TradeAggregator::addTrade()          (WebSocket thread, mutex-guarded)
        │  bucket into window keyed by (symbol, floor(time / windowMs) * windowMs)
        │  update: trades++, volume += price*qty, min/max price, buy/sellCount
        ▼
[every serializationIntervalMs — timer thread]
TradeAggregator::popCompletedWindows(nowMs)
        │  returns windows where windowStart + windowMs ≤ nowMs
        ▼
FileSerializer::write(windows)
        │  append to outputFile, one block per timestamp:
        │
        │  timestamp=2026-03-11T15:30:45Z
        │  symbol=BTCUSDT trades=142 volume=12345.6789 min=42000.0000 max=42500.0000 buy=75 sell=67
        │  symbol=ETHUSDT trades=89  volume=5678.1234  min=2200.0000  max=2250.0000  buy=45 sell=44
        ▼
[on SIGINT / SIGTERM]
MonitoringService::stopMonitoring()
        │  final flush of all remaining in-flight windows
        └─► clean shutdown
```

---

## Configuration

The service reads a JSON file at startup (default: `config.json` in the working directory).

```json
{
  "tradingPairs":           ["BTCUSDT", "ETHUSDT"],
  "host":                   "stream.binance.com",
  "port":                   "9443",
  "windowMs":               1000,
  "serializationIntervalMs": 5000,
  "outputFile":             "market_data.log",
  "logLevel":               "info",
  "reconnectMaxDelayMs":    60000
}
```

| Key | Required | Default | Description |
|-----|----------|---------|-------------|
| `tradingPairs` | yes | — | List of Binance symbols to subscribe to |
| `host` | no | `stream.binance.com` | WebSocket server hostname |
| `port` | no | `9443` | WebSocket server port |
| `windowMs` | yes | — | Aggregation window width in milliseconds |
| `serializationIntervalMs` | yes | — | How often completed windows are flushed to disk |
| `outputFile` | no | `market_data.log` | Output file path (opened in append mode) |
| `logLevel` | no | `info` | One of: `trace` `debug` `info` `warn` `error` `critical` `off` |
| `reconnectMaxDelayMs` | no | `60000` | Maximum reconnection back-off delay in milliseconds |

All missing required keys cause an immediate `std::runtime_error` at startup.

---

## Usage

```bash
BinanceDataCollector [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `-c <path>` | Path to JSON config file |
| `--config <path>` | Path to JSON config file |
| `--config=<path>` | Path to JSON config file (equals form) |

Default config path is `config.json` in the working directory.

```bash
# Use default config.json in the current directory
./BinanceDataCollector

# Provide a custom config path
./BinanceDataCollector -c /etc/bdc/production.json
./BinanceDataCollector --config /etc/bdc/production.json
./BinanceDataCollector --config=/etc/bdc/production.json
```

---

## Build

### Prerequisites

- CMake ≥ 3.10
- Conan 2
- C++20-capable compiler (GCC 12+, Clang 15+, MSVC 17.4+)
- OpenSSL dev libraries

### Release build

```bash
conan install . --build=missing
cmake -S . --preset conan-release
cmake --build build --preset conan-release
```

Binary: `build/Release/src/app/BinanceDataCollector`

### Debug build

```bash
conan install . --build=missing -s build_type=Debug
cmake -S . --preset conan-debug
cmake --build build/Debug --preset conan-debug
```

Binary: `build/Debug/src/app/BinanceDataCollector`

---

## Testing & Code Coverage

### Run all unit tests

```bash
cd build && ctest -C Release
```

Or run the binary directly:

```bash
./build/tests/Release/BinanceTests.exe
```

Run a single test:

```bash
./build/tests/Release/BinanceTests.exe --gtest_filter="ArgParserTest.ShortFlagSetsConfigPath"
```

### Integration tests (require live Binance connection)

```bash
./build/tests/Release/BinanceIntegrationTests.exe
```

### Code coverage (lcov + HTML report)

Requires `lcov`:

```bash
sudo apt install lcov
```

Configure and build with coverage instrumentation:

```bash
conan install . --build=missing -s build_type=Debug
cmake -S . --preset coverage
cmake --build build/Debug --preset coverage
```

Generate the report (runs both unit and integration tests):

```bash
cmake --build build/Debug --target coverage
```

Output:

- **HTML report:** `build/Debug/coverage-report/html/index.html`
- **Terminal summary:** printed at the end of the build

The `coverage` target zeros counters, runs `BinanceTests` and `BinanceIntegrationTests`, captures with `lcov` (filtering out system headers, Conan cache, and test source files), and generates the HTML report via `genhtml`.

---

## Deploy (Linux — systemd)

### 1. Create a dedicated user and install files

```bash
sudo useradd -r -s /sbin/nologin bdc
sudo mkdir -p /opt/bdc
sudo cp build/Release/src/app/BinanceDataCollector /opt/bdc/
sudo cp config.json /opt/bdc/
sudo chown -R bdc:bdc /opt/bdc
```

### 2. Install and enable the service

```bash
sudo cp deploy/linux/binancedatacollector.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now binancedatacollector
```

### 3. Check status and logs

```bash
sudo systemctl status binancedatacollector
journalctl -u binancedatacollector -f
```

### 4. Stop / restart

```bash
sudo systemctl stop binancedatacollector
sudo systemctl restart binancedatacollector
```

### 5. Use a custom config path

Edit the service file to pass `-c`:

```ini
ExecStart=/opt/bdc/BinanceDataCollector -c /opt/bdc/production.json
```

Then reload:

```bash
sudo systemctl daemon-reload
sudo systemctl restart binancedatacollector
```

### Service resource limits

The bundled unit file applies the following limits:

| Setting | Value |
|---------|-------|
| Restart | `on-failure` with 5 s delay |
| Max restarts | 5 in 60 s |
| Stop timeout | 30 s (then SIGKILL) |
| Open files | 65 536 |
| Memory cap | 256 MiB |
