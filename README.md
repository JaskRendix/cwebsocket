# **cwebsocket**

Lightweight, portable WebSocket server library in C, derived from the original *cwebsocket* project and modernized for strict RFC 6455 compliance, modularity, and testability.

## **Features**

- Pure C implementation  
- Small, self‑contained codebase  
- No external dependencies  
- Server‑side RFC 6455 handshake and framing  
- Strict opcode, FIN, RSV, masking, and length validation  
- Continuation‑frame and fragmented‑message support  
- Zero‑copy streaming callbacks  
- Suitable for embedded and microcontroller targets  
- MIT license  

---

## **Protocol correctness**

The library implements the core requirements of RFC 6455 and enforces them consistently across the builder, parser, continuation logic, and streaming layer:

- Correct Sec‑WebSocket‑Accept generation  
- Correct handling of 7‑bit, 16‑bit, and 64‑bit payload lengths  
- Validation of 64‑bit lengths (MSB must be zero)  
- Mandatory masking for client frames  
- Rejection of unmasked client frames  
- Rejection of reserved opcodes  
- Rejection of non‑zero RSV bits  
- Control‑frame rules: FIN=1, payload ≤125 bytes  
- CLOSE frame rules: 2‑byte code + reason ≤123 bytes  
- Deterministic continuation‑frame assembly  
- Deterministic streaming behavior with no undefined states  

All modules build warning‑free under modern compilers.

---

## **Modular architecture**

The original monolithic source file has been split into focused, testable modules:

- **handshake.c** — HTTP Upgrade parsing and response generation  
- **frame_builder.c** — server and client frame construction with RFC‑compliant validation  
- **frame_parser.c** — single‑frame parsing, unmasking, and strict protocol checks  
- **continuation.c** — fragmented message assembly and continuation‑frame validation  
- **streaming.c** — zero‑copy streaming callbacks and incremental frame processing  
- **consume.c** — TCP buffer walker and frame dispatch  

This structure improves clarity, maintainability, and correctness while preserving API compatibility.

---

## **Continuation frames and streaming**

The library supports incremental and fragmented message processing:

- RFC‑compliant continuation frames  
- Fragmented message assembly via `wsMessageContext`  
- Zero‑copy streaming via `wsStreamCallbacks`  
- Correct handling of TEXT, BINARY, PING, PONG, and CLOSE  
- Validation of illegal continuation patterns  
- Validation of illegal new‑data frames during fragmentation  
- Capacity‑checked assembly of fragmented messages  

A modernized x86 echo server demonstrates the streaming API and is covered by an integration test.

---

## **Testing**

The test suite covers:

- SHA‑1  
- Base64  
- Handshake  
- Frame building  
- Frame parsing  
- Continuation logic (valid and invalid)  
- Streaming logic (valid and invalid)  
- x86 echo server integration  

All tests pass under modern toolchains.

---

## **Microcontroller suitability**

The library remains appropriate for embedded and microcontroller environments:

- No dynamic allocation required  
- No external dependencies  
- Small code footprint  
- Deterministic behavior  
- Configurable buffer ownership  

It can expose device data to a browser through a WebSocket connection with minimal overhead.

---

## **Not supported**

- Secure WebSocket (wss)  
- WebSocket extensions  
- WebSocket subprotocols  
- Cookies and authentication headers  
- Status codes beyond CLOSE  

---

## **Building**

```
mkdir build
cd build
cmake ..
make
```

---

## **Reference**

Original project:  
[https://github.com/m8rge/cwebsocket](https://github.com/m8rge/cwebsocket)
