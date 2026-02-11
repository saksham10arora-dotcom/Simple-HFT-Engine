=# ⚡ Ultra-Fast C++ HFT Matching Engine

A high-performance order matching engine designed for low-latency trading environments. This project focuses on throughput and minimal execution overhead, typical of High-Frequency Trading (HFT) systems.



## 🚀 Performance Metrics
* **Throughput:** ~5.5M+ Transactions Per Second (TPS)
* **Priority:** Price-Time (FIFO) Execution
* **Memory:** Minimal heap allocation for low-latency stability
* **Architecture:** Hot-Standby ready for burst order flow

## 🛠️ Build & Run (30-Second Start)
Requires a C++17 compliant compiler and CMake.

### On Windows (CMD/PowerShell):
```bash
# Navigate to project directory
cd HFT_Engine

# Create and enter build directory
mkdir build
cd build

# Generate build files and compile
cmake ..
cmake --build . --config Release

# Run the engine
./HFT_Engine
