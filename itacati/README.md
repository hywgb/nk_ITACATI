# ITACATI (assumed) – SIP/RTC Platform

Note: We found no public documentation for “上海南康科技 ITACATI”. This repo scaffolds an ITACATI-like system following industry best practices for SIP/WebRTC:
- Frontend: React + TypeScript using JsSIP over WebSocket (RFC 7118)
- Backend: C++17 service using PJSIP/PJSUA2
- NAT traversal: ICE with STUN/TURN
- Security: TLS for SIP signaling; SRTP (DTLS-SRTP or SDES) for media

## Architecture (high level)
- Web client (React): Uses JsSIP to register/place calls via SIP over WSS. Media is WebRTC.
- SIP WS gateway: A SIP proxy with WebSocket transport (e.g., Kamailio websocket module) terminates `wss://` and routes SIP to the SIP core.
- SIP core (C++17, PJSIP): Business logic/B2BUA/registrar/IVR hooks. Can also act as a UA to external trunks.
- NAT traversal helpers: STUN and (optional) TURN server (e.g., coturn).
- Media: End-to-end WebRTC between browsers; interop via RTP/SRTP when needed.

This scaffold includes the React client and a PJSUA2-based C++ service. In production, deploy a WebSocket-capable SIP proxy (e.g., Kamailio) in front of the PJSIP core.

## Repo layout
- `frontend/`: React + TypeScript client (JsSIP)
- `backend/`: C++17 PJSUA2 service (CMake)

## Prerequisites
- Node 20+ / pnpm or npm (for `frontend`)
- A SIP WebSocket endpoint (WSS) reachable by the browser (e.g., Kamailio with `websocket` module)
- STUN/TURN servers (e.g., `stun:stun.l.google.com:19302`, your own `turn:` on coturn)
- For backend: a system with toolchain + CMake + PJSIP (pjproject) available (or build via Dockerfile)

## Quick start
### Frontend
```
cd frontend
pnpm install  # or npm install
echo "VITE_SIP_WSS=wss://your-sip-ws.example.com:7443" > .env.local
pnpm dev      # or npm run dev
```
Open http://localhost:5173 and configure SIP credentials.

### Backend (local build)
1) Ensure pjproject (PJSIP) is installed or available for CMake find. Alternatively, use the provided Dockerfile to build pjproject.
2) Build:
```
cd backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/itacati-core
```
Environment variables (see `backend/config.example.env`) drive basic account/transport setup for demo registration/testing.

## Best practices baked in
- RFC 7118 SIP over WebSocket for browsers
- ICE (with STUN/TURN) for NAT traversal
- TLS for SIP (tcp/tls) and WSS; SRTP for media
- Separation of WS transport (proxy) and SIP core (C++) for maintainability and scale

## Next steps
- Stand up Kamailio with `websocket` and `outbound`/`path` modules; route to backend
- Deploy coturn for TURN/TCP in restrictive networks
- Add auth (JWT -> SIP credentials mapping) and webhooks to backend
- Observability (Prometheus/OpenTelemetry) and structured logging