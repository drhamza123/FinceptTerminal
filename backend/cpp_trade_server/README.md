# Fincept MT5 Trade Server

C++ trade server for ultra-low-latency MT5 order execution.
- 64-bit native Windows executable
- Netting & Hedging mode support
- In-memory order book & position tracking
- ZMQ bridge to Python backend
- Direct MT5 terminal communication via Named Pipes

## Build

```
cl /EHsc /std:c++20 /D_WIN32_WINNT=0x0A00 trade_server.cpp /link /OUT:trade_server.exe
```

Or use CMake with the included CMakeLists.txt.
