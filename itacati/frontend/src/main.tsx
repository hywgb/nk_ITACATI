import React, { useEffect, useRef, useState } from 'react'
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

  // ICE servers 配置
  const [stun, setStun] = useState('stun:stun.l.google.com:19302')
  const [turn, setTurn] = useState('') // 例：turn:turn.example.com:3478?transport=tcp
  const [turnUser, setTurnUser] = useState('')
  const [turnPass, setTurnPass] = useState('')

  const uaRef = useRef<any>(null)
  const sessionRef = useRef<any>(null)
  const remoteAudioRef = useRef<HTMLAudioElement>(null)

  useEffect(() => {
    return () => {
      try { uaRef.current?.stop() } catch { /* ignore */ }
    }
  }, [])

  const buildIceServers = () => {
    const arr: any[] = []
    if (stun) arr.push({ urls: stun })
    if (turn) arr.push({ urls: turn, username: turnUser || undefined, credential: turnPass || undefined })
    return arr
  }

  const startUA = () => {
    if (!wsUri || !sipUri || !authUser || !password) return

    const socket = new JsSIP.WebSocketInterface(wsUri)
    const configuration: any = {
      sockets: [socket],
      uri: sipUri,
      authorization_user: authUser,
      password,
      session_timers: false
    }

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
      pcConfig: { iceServers: buildIceServers() },
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
    <div style={{ fontFamily: 'sans-serif', maxWidth: 900, margin: '20px auto' }}>
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
        <label>STUN</label>
        <input value={stun} onChange={e => setStun(e.target.value)} placeholder="stun:stun.l.google.com:19302" />
        <label>TURN</label>
        <input value={turn} onChange={e => setTurn(e.target.value)} placeholder="turn:turn.example.com:3478?transport=tcp" />
        <label>TURN User</label>
        <input value={turnUser} onChange={e => setTurnUser(e.target.value)} />
        <label>TURN Pass</label>
        <input type="password" value={turnPass} onChange={e => setTurnPass(e.target.value)} />
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
        说明：需要支持 RFC 7118 的 SIP 网关（如 Kamailio websocket）。可配置 STUN/TURN，提高 NAT 场景下建链成功率。
      </p>
    </div>
  )
}

const root = createRoot(document.getElementById('root')!)
root.render(<App />)