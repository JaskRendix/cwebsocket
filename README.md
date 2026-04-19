# **cwebsocket**

A lightweight, portable WebSocket server library in C.  
Modernized for strict RFC 6455 compliance, modularity, and deterministic behavior.

## **Features**

- Pure C implementation  
- No external dependencies  
- RFC 6455‑compliant handshake and framing  
- Strict opcode, FIN, RSV, masking, and length validation  
- Fragmented‑message and continuation‑frame support  
- Zero‑copy streaming callbacks  
- Deterministic parsing and error handling  
- Suitable for embedded and microcontroller targets  
- MIT license  

---

## **Protocol correctness**

The library enforces the core requirements of RFC 6455 across all modules:

- Valid Sec‑WebSocket‑Accept computation  
- 7‑bit, 16‑bit, and 64‑bit payload length handling  
- 64‑bit length validation (MSB must be zero)  
- Mandatory masking for client frames  
- Rejection of unmasked client frames  
- Rejection of reserved opcodes and non‑zero RSV bits  
- Control‑frame rules: FIN=1, payload ≤125 bytes  
- CLOSE frame rules: 2‑byte code + optional reason ≤123 bytes  
- Deterministic continuation‑frame assembly  
- Strict enforcement of fragmented‑message rules:
  - Start: FIN=0, opcode=TEXT/BINARY  
  - Middle: FIN=0, opcode=0x00  
  - Final: FIN=1, opcode=0x00  
  - Rejection of illegal continuation starts  
  - Rejection of illegal new‑data frames during fragmentation  
- Deterministic streaming behavior with no undefined states  

All modules build warning‑free under modern compilers.

---

## **Modular architecture**

The codebase is organized into focused, testable modules:

- **handshake.c** — HTTP Upgrade parsing and response generation  
- **frame_builder.c** — server and client frame construction with masking and length encoding  
- **frame_parser.c** — single‑frame parsing, unmasking, opcode/RSV validation, and length checks  
- **continuation.c** — fragmented‑message assembly and continuation‑frame validation  
- **streaming.c** — zero‑copy streaming callbacks and incremental frame processing  
- **consume.c** — TCP buffer walker, frame dispatch, and protocol‑error propagation  
- **base64.c** — corrected Base64 encoder with null termination and bounds checking  

This structure improves clarity, maintainability, and testability.

---

## **Continuation frames and streaming**

The library supports incremental and fragmented message processing:

- RFC‑compliant continuation frames  
- Fragmented‑message assembly via `wsMessageContext`  
- Capacity‑checked buffering  
- Zero‑copy streaming via `wsStreamCallbacks`  
- Correct handling of TEXT, BINARY, PING, PONG, and CLOSE  
- Strict validation of illegal continuation patterns  

An x86 echo server demonstrates the streaming API and is covered by an integration test.

---

## **Testing**

The test suite covers:

- SHA‑1  
- Base64 (corrected encoder)  
- Handshake  
- Frame building  
- Frame parsing (opcode, RSV, masking, length validation)  
- Continuation logic (valid and invalid)  
- Streaming logic (valid and invalid)  
- Buffer consumption (multi‑frame, partial frames, protocol errors)  
- x86 echo server integration  

All tests pass under modern toolchains.

---

## **Microcontroller suitability**

The library is suitable for embedded environments:

- No dynamic allocation required  
- No external dependencies  
- Small code footprint  
- Deterministic behavior  
- Configurable buffer ownership  

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
