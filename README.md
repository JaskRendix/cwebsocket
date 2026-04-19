# **cwebsocket**

A lightweight, portable WebSocket server library in C.  
Modernized for strict RFC 6455 compliance, modularity, and deterministic behavior.

## **Features**

- Pure C implementation  
- No external dependencies  
- RFC 6455‑compliant handshake and framing  
- Strict opcode, FIN, RSV, masking, and length validation  
- Full control‑frame safety (PING, PONG, CLOSE)  
- Fragmented‑message and continuation‑frame support  
- Zero‑copy streaming callbacks  
- Deterministic parsing and error handling  
- Portable `ntohs/htons` fallback for Arduino/Windows  
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

### **Control‑frame rules (PING, PONG, CLOSE)**

- FIN **must** be 1  
- Payload length **≤125 bytes**  
- Control frames **must not** be fragmented  
- Control frames **must not** appear inside fragmented messages  
- PONG frames echo the PING payload exactly (`wsMakePongFrame`)  
- Invalid control frames are rejected consistently in:
  - `frame_parser.c`  
  - `consume.c`  
  - `continuation.c`  

### **Fragmented‑message rules**

- Start: FIN=0, opcode=TEXT/BINARY  
- Middle: FIN=0, opcode=0x00  
- Final: FIN=1, opcode=0x00  
- Illegal continuation starts are rejected  
- New TEXT/BINARY frames during fragmentation are rejected  
- Capacity‑checked assembly into user‑provided buffers  

All modules build warning‑free under modern compilers.

---

## **Modular architecture**

The codebase is organized into focused, testable modules:

- **handshake.c** — HTTP Upgrade parsing and response generation  
- **frame_builder.c** — frame construction, masking, length encoding, `wsMakePongFrame`  
- **frame_parser.c** — single‑frame parsing, unmasking, opcode/RSV validation, length checks  
- **continuation.c** — fragmented‑message assembly and continuation‑frame validation  
- **consume.c** — TCP buffer walker, frame dispatch, and protocol‑error propagation  
- **streaming.c** — zero‑copy streaming callbacks and incremental frame processing  
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

## **Control‑frame handling**

The library implements strict RFC 6455 control‑frame semantics:

- PING, PONG, and CLOSE must be unfragmented  
- Payload length must not exceed 125 bytes  
- PONG frames mirror the PING payload  
- Control frames are rejected during fragmented messages  
- Validation is enforced in all parsing paths (stateless and stateful)  

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
- Control‑frame handling (PING, PONG, CLOSE)  
- Rejection of fragmented or oversized control frames  
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
- Portable networking layer for Arduino and Windows  

---

## **Not supported**

- Secure WebSocket (wss)  
- WebSocket extensions  
- WebSocket subprotocols  
- Cookies and authentication headers  
- Status codes beyond CLOSE  
- Automatic fragmentation of outgoing frames  

---

## **Building**

```
mkdir build
cd build
cmake ..
make
```

---

## **Arduino**

An Arduino‑compatible library package can be generated using the provided script:

```
build_arduino_library.sh
```

This produces a standard Arduino 1.5+ library layout with examples.

---

## **Reference**

Original project:  
[https://github.com/m8rge/cwebsocket](https://github.com/m8rge/cwebsocket)
