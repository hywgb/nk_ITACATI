import React, { useEffect, useMemo, useRef, useState } from 'react'
import { createRoot } from 'react-dom/client'
import JsSIP from 'jssip'

const App: React.FC = () => {
  const [wsUri, setWsUri] = useState<string>(import.meta.env.VITE_SIP_WSS || '')
  const [sipUri, setSipUri] = useState('sip:alice@example.com')
  const [authUser, setAuthUser] = useState('alice')
  const [password, setPassword] = useState('changeme')
  const [dst, setDst] = useState('sip:bob@example.com')
  const [status, setStatus] = useState<string>('idle')
  const [registered, setRegistered] = useState(false)
  const [inCall, setInCall] = useState(false)

  const uaRef = useRef<any>(null)
  const sessionRef = useRef<any>(null)
  const remoteAudioRef = useRef<HTMLAudioElement>(null)

  useEffect(() => {
    return () => {
      try { uaRef.current?.stop() } catch { /* ignore */ }
    }
  }, [])

  const startUA = () => {
    if (!wsUri || !sipUri || !authUser || !password) return

    const socket = new JsSIP.WebSocketInterface(wsUri)
    const configuration = {
      sockets: [socket],
      uri: sipUri,
      authorization_user: authUser,
      password,
      session_timers: false
    } as any

    const ua = new JsSIP.UA(configuration)
    ua.on('connected', () => setStatus('ws connected'))
    ua.on('disconnected', () => { setStatus('ws disconnected'); setRegistered(false) })
    ua.on('registered', () => { setRegistered(true); setStatus('registered') })
    ua.on('unregistered', () => { setRegistered(false); setStatus('unregistered') })
    ua.on('registrationFailed', (e: any) => setStatus(`reg failed: ${e.cause}`))

    ua.on('newRTCSession', (e: any) => {
      const session = e.session
      sessionRef.current = session

      session.on('peerconnection', (ev: any) => {
        const pc: RTCPeerConnection = ev.peerconnection
        pc.ontrack = (te: RTCTrackEvent) => {
          if (remoteAudioRef.current && te.streams[0]) {
            remoteAudioRef.current.srcObject = te.streams[0]
          }
        }
      })

      session.on('progress', () => setStatus('progress'))
      session.on('accepted', () => { setInCall(true); setStatus('in call') })
      session.on('failed', (ev: any) => { setInCall(false); setStatus(`failed: ${ev.cause}`) })
      session.on('ended', () => { setInCall(false); setStatus('ended') })
    })

    ua.start()
    uaRef.current = ua
  }

  const register = () => uaRef.current?.register()
  const unregister = () => uaRef.current?.unregister()

  const call = async () => {
    if (!uaRef.current) return
    const options: any = {
      mediaConstraints: { audio: true, video: false },
      rtcOfferConstraints: {
        offerToReceiveAudio: 1,
        offerToReceiveVideo: 0
      }
    }
    uaRef.current.call(dst, options)
  }

  const hangup = () => sessionRef.current?.terminate()
  const sendDTMF = (tone: string) => sessionRef.current?.sendDTMF(tone)

  return (
    <div style={{ fontFamily: 'sans-serif', maxWidth: 800, margin: '20px auto' }}>
      <h2>ITACATI Web Client (React + JsSIP)</h2>
      <div style={{ display: 'grid', gridTemplateColumns: '180px 1fr', gap: 8 }}>
        <label>WSS URI</label>
        <input value={wsUri} onChange={e => setWsUri(e.target.value)} placeholder="wss://sip.example.com:7443" />
        <label>SIP URI</label>
        <input value={sipUri} onChange={e => setSipUri(e.target.value)} placeholder="sip:alice@example.com" />
        <label>Auth User</label>
        <input value={authUser} onChange={e => setAuthUser(e.target.value)} />
        <label>Password</label>
        <input type="password" value={password} onChange={e => setPassword(e.target.value)} />
      </div>
      <div style={{ marginTop: 10 }}>
        <button onClick={startUA}>Init</button>
        <button onClick={register} disabled={!uaRef.current || registered}>Register</button>
        <button onClick={unregister} disabled={!uaRef.current || !registered}>Unregister</button>
      </div>

      <hr />
      <div>
        <label>Destination</label>{' '}
        <input value={dst} onChange={e => setDst(e.target.value)} style={{ width: 360 }} />
        <button onClick={call} disabled={!registered || inCall}>Call</button>
        <button onClick={hangup} disabled={!inCall}>Hangup</button>
        <button onClick={() => sendDTMF('1')} disabled={!inCall}>DTMF 1</button>
      </div>

      <div style={{ marginTop: 10 }}>Status: <b>{status}</b></div>
      <audio ref={remoteAudioRef} autoPlay />
      <p style={{ fontSize: 12, color: '#666' }}>
        Note: Requires a SIP proxy that supports SIP over WebSocket (RFC 7118), e.g., Kamailio with `websocket` module.
      </p>
    </div>
  )
}

const root = createRoot(document.getElementById('root')!)
root.render(<App />)