# cwebsocket

Lightweight, portable WebSocket server library in C, inspired by the original cwebsocket project.

## Features

- Pure C implementation  
- Small, modular codebase  
- No external dependencies  
- Server‑side RFC 6455 handshake and framing  
- Continuation‑frame and fragmented‑message support  
- Streaming callbacks for zero‑copy message processing  
- Suitable for embedded and microcontroller targets  
- MIT license  

## Protocol correctness and portability

The implementation is designed for strict RFC 6455 compliance and predictable behavior across modern toolchains:

- Fixed‑width, header‑only SHA‑1 implementation  
- Bounds‑checked, header‑only Base64 implementation  
- Modern handshake parsing  
- Correct Sec‑WebSocket‑Accept generation  
- Correct handling of 7‑bit, 16‑bit, and 64‑bit payload lengths  
- Strict opcode, FIN, and masking validation  
- No implicit declarations or undefined behavior  
- Warning‑free builds under modern compilers  

The public API remains compatible with the original repository.

## Modular architecture

The original monolithic source file has been split into focused modules:

- `handshake.c` — HTTP Upgrade parsing and response generation  
- `frame_builder.c` — server and client frame construction  
- `frame_parser.c` — single‑frame parsing and unmasking  
- `continuation.c` — fragmented message assembly  
- `streaming.c` — zero‑copy streaming callbacks  
- `consume.c` — TCP buffer walker and frame dispatch  

This separation improves readability, testability, and maintainability without altering behavior.

## Continuation frames and streaming

The library supports incremental and fragmented message processing:

- RFC‑compliant continuation frames  
- Fragmented message assembly via `wsMessageContext`  
- Zero‑copy streaming via `wsStreamCallbacks`  
- Correct CLOSE, PING, and PONG handling  
- Modernized x86 echo server using the streaming API  
- Socket‑level integration test  

## Microcontrollers

The library remains suitable for microcontroller and embedded use.  
It can expose device data to a browser through a WebSocket connection with minimal memory overhead.

## Not supported

- Secure WebSocket (wss)  
- WebSocket extensions  
- WebSocket subprotocols  
- Cookies and authentication headers  
- Status codes beyond CLOSE  

## Building

```
mkdir build
cd build
cmake ..
make
```

## Reference

Original project: [https://github.com/m8rge/cwebsocket](https://github.com/m8rge/cwebsocket)
