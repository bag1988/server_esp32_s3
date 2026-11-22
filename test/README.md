# Testing Guide

## Unit Tests (Host)

Run unit tests on the host machine (no ESP32 needed):

```bash
# Build and run all unit tests
pio test -e test

# Or manually:
cd test
mkdir build && cd build
cmake ..
make
./test_runner
```

### Test Coverage

| Test File | Description |
|-----------|-------------|
| `crc32_test.cpp` | CRC32 checksum verification (table-based algorithm) |
| `control_gpio_test.cpp` | Core heating control logic (hysteresis, auto/manual GPIO, device offline detection) |
| `device_manager_test.cpp` | DeviceManager command handling (CRUD operations, sensor updates, config changes) |
| `nvs_storage_test.cpp` | NVS storage mock (string/blob/u32/u64 save/load) |

### Mock Framework

The tests use a custom Arduino/FreeRTOS mock in `test/mocks/`:
- `arduino_mocks.h/cpp` — Serial, GPIO, WiFi, NVS, FreeRTOS tasks/queues/semaphores
- `mock_millis` — controllable time for testing timeouts

## Integration Tests (ESP32 Device)

Run on actual ESP32-S3 hardware:

```bash
# Build, upload, and monitor
pio test -e integration
```

### Test Coverage

| Test | Description |
|------|-------------|
| `test_device_manager_init` | DeviceManager task starts correctly |
| `test_device_manager_cmd_roundtrip` | Add/Get device via command queue |
| `test_nvs_storage_roundtrip` | WiFi credentials save/load with CRC |
| `test_crc32_verification` | CRC32 consistency and sensitivity |
| `test_task_monitor_registration` | Task registration and health check |
| `test_gpio_control_logic` | Hardware GPIO read/write |
| `test_ble_scanner_task` | BLE scanner task running |
| `test_web_server_endpoints` | Web server responsiveness |

## Running Specific Tests

```bash
# Run only unit tests
pio test -e test

# Run only integration tests  
pio test -e integration

# Run with verbose output
pio test -e test -v
```

## Adding New Tests

1. **Unit test**: Add `.cpp` to `test/unit/` with `TEST()` or `TEST_F()` macros
2. **Integration test**: Add test function to `test/integration/integration_tests.cpp` and call from `run_integration_tests()`

## CI/CD

Example GitHub Actions workflow (`.github/workflows/test.yml`):

```yaml
name: Tests
on: [push, pull_request]
jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
      - run: pip install platformio
      - run: pio test -e test
  
  integration-tests:
    runs-on: self-hosted  # Requires ESP32 hardware
    steps:
      - uses: actions/checkout@v4
      - run: pip install platformio
      - run: pio test -e integration
```