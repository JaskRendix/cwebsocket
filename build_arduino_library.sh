#!/bin/bash

# Setup Structure
mkdir -p arduino_library/websocket/src
mkdir -p arduino_library/websocket/examples/echo

# Flatten Library into 'src' (Arduino 1.5+ standard)
cp lib/*.c arduino_library/websocket/src/
cp lib/*.h arduino_library/websocket/src/

# Copy Metadata and Examples
cp arduino_server/keywords.txt arduino_library/websocket/
cp arduino_server/arduino_server.ino arduino_library/websocket/examples/echo/echo.ino

# Generate library.properties
cat <<EOT > arduino_library/websocket/library.properties
name=cwebsocket
version=2.0.0
author=Putilov Andrey
maintainer=Giorgio
sentence=A lightweight, RFC 6455 compliant WebSocket server library in C.
paragraph=Modernized for strict compliance, modularity, and deterministic behavior. Suitable for embedded and microcontroller targets.
category=Communication
url=https://github.com/m8rge/cwebsocket
architectures=*
EOT

echo "Arduino library built successfully in arduino_library/websocket"