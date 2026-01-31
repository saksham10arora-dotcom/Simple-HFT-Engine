# Ultra-Low Latency HFT Matching Engine

A high-performance C++ Limit Order Book (LOB) designed for High-Frequency Trading. This engine focuses on deterministic performance and minimal latency using custom memory management.

## 🚀 Performance
- **Throughput:** ~5.5 Million orders per second
- **Latency:** Sub-10ns allocation via custom Memory Pool
- **Build System:** CMake 3.10+

## 🛠️ Technical Highlights
- **Memory Pool:** Custom-built template-based pool to eliminate heap fragmentation and `new/delete` overhead during the hot path.
- **Order Matching:** Implements Price-Time Priority using `std::map` for optimized limit price discovery.
- **Zero-Copy Architecture:** Uses pointer-passing and pre-allocated buffers to maintain high cache-locality.

## 📂 Project Structure
- `include/`: Header files (Logic & Memory Management)
- `src/`: Core implementation
- `build/`: Compiled binaries

## 💻 How to Run
1. Navigate to `/build`
2. Run `cmake ..`
3. Build: `cmake --build . --config Release`
4. Execute: `Release\hft_engine.exe`