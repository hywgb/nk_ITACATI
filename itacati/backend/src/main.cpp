#include <pjsua2.hpp>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <chrono>

using namespace pj;

static std::string getenvOr(const char* k, const char* defv) {
  const char* v = std::getenv(k);
  return v ? std::string(v) : std::string(defv);
}

static bool getenvBool(const char* k, bool defv=false) {
  std::string v = getenvOr(k, defv ? "1" : "");
  if (v.empty()) return false;
  std::string s = v; std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return (s=="1"||s=="true"||s=="yes"||s=="on");
}

struct MyAccount : public Account {
  void onRegState(OnRegStateParam &prm) override {
    std::cout << "[Account] onRegState: code=" << prm.code << ", reason=" << prm.reason << std::endl;
  }
  void onIncomingCall(OnIncomingCallParam &prm) override {
    std::cout << "[Account] Incoming call, auto-answer 200" << std::endl;
    Call *call = new Call(*this, prm.callId);
    CallOpParam ans;
    ans.statusCode = (pjsip_status_code)200;
    try { call->answer(ans); } catch(...) { }
    delete call;
  }
};

struct MyCall : public Call {
  using Call::Call;
  void onCallState(OnCallStateParam &prm) override {
    PJ_UNUSED_ARG(prm);
    CallInfo ci = getInfo();
    std::cout << "[Call] state=" << ci.stateText << ", lastCode=" << ci.lastStatusCode << "(" << ci.lastReason << ")" << std::endl;
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
      std::cout << "[Call] disconnected." << std::endl;
    }
  }
  void onCallMediaState(OnCallMediaStateParam &prm) override {
    PJ_UNUSED_ARG(prm);
    CallInfo ci = getInfo();
    for (unsigned i=0; i<ci.media.size(); ++i) {
      if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE) {
        std::cout << "[Call] audio active." << std::endl;
      }
    }
  }
};

int main() {
  Endpoint ep;
  try {
    ep.libCreate();

    EpConfig epcfg;
    epcfg.uaConfig.maxCalls = 16;
    epcfg.logConfig.level = 4;
    epcfg.logConfig.consoleLevel = 4;

    // 媒体与NAT配置（ICE/TURN/SRTP/VAD）
    bool iceOn = getenvBool("ITACATI_ICE", true);
    bool vadOff = getenvBool("ITACATI_NO_VAD", false);
    std::string srtpMode = getenvOr("ITACATI_SRTP", "optional"); // disabled/optional/mandatory
    std::string turnUrl = getenvOr("ITACATI_TURN_URL", "");     // e.g. turn:turn.example.com:3478?transport=tcp
    std::string turnUser = getenvOr("ITACATI_TURN_USER", "");
    std::string turnPass = getenvOr("ITACATI_TURN_PASS", "");
    std::string turnConn = getenvOr("ITACATI_TURN_CONN", "tcp"); // udp/tcp/tls

    epcfg.medConfig.noVad = vadOff;
    epcfg.medConfig.iceEnabled = iceOn;

    if (!turnUrl.empty()) {
      epcfg.medConfig.turnEnabled = true;
      epcfg.medConfig.turnServer = turnUrl;
      epcfg.medConfig.turnConnType = (turnConn=="tls" ? pjsua_turn_conn_type::PJSUA_TURN_TPT_TLS : (turnConn=="tcp" ? pjsua_turn_conn_type::PJSUA_TURN_TPT_TCP : pjsua_turn_conn_type::PJSUA_TURN_TPT_UDP));
      if (!turnUser.empty()) epcfg.medConfig.turnUserName = turnUser;
      if (!turnPass.empty()) epcfg.medConfig.turnPassword = turnPass;
    }

    if (srtpMode=="disabled") epcfg.medConfig.srtpUse = 0; // SRTP_DISABLED
    else if (srtpMode=="mandatory") epcfg.medConfig.srtpUse = 2; // SRTP_MANDATORY
    else epcfg.medConfig.srtpUse = 1; // SRTP_OPTIONAL

    ep.libInit(epcfg);

    // Transports
    TransportConfig tcfg;
    tcfg.port = (unsigned)std::stoi(getenvOr("ITACATI_SIP_PORT", "5060"));
    try { ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg); } catch(...) {}

    TransportConfig tcfgTcp; tcfgTcp.port = (unsigned)std::stoi(getenvOr("ITACATI_SIP_TCP_PORT", "5060"));
    try { ep.transportCreate(PJSIP_TRANSPORT_TCP, tcfgTcp); } catch(...) {}

    if (!getenvOr("ITACATI_SIP_TLS", "").empty()) {
      TransportConfig tcfgTls; tcfgTls.port = (unsigned)std::stoi(getenvOr("ITACATI_SIP_TLS_PORT", "5061"));
      try { ep.transportCreate(PJSIP_TRANSPORT_TLS, tcfgTls); } catch(...) {}
    }

    ep.libStart();
    std::cout << "PJSUA2 started. ICE=" << iceOn << ", SRTP=" << srtpMode << ", TURN=" << (!turnUrl.empty()) << std::endl;

    // Account（可选注册）
    std::string sipUser = getenvOr("ITACATI_SIP_USER", "");
    std::string sipDomain = getenvOr("ITACATI_SIP_DOMAIN", "");
    std::string authPass = getenvOr("ITACATI_SIP_PASS", "");
    std::string proxy = getenvOr("ITACATI_SIP_PROXY", ""); // 例如 sip:proxy.example.com;transport=tcp

    std::unique_ptr<MyAccount> acc;
    if (!sipUser.empty() && !sipDomain.empty()) {
      AccountConfig accCfg;
      accCfg.idUri = "sip:" + sipUser + "@" + sipDomain;
      if (!proxy.empty()) accCfg.sipConfig.proxies = StringVector{ proxy };
      if (!authPass.empty()) {
        AuthCredInfo cred("digest", sipDomain, sipUser, 0, authPass);
        accCfg.sipConfig.authCreds.push_back(cred);
      }
      accCfg.regConfig.registrarUri = "sip:" + sipDomain;

      acc = std::make_unique<MyAccount>();
      acc->create(accCfg);
    }

    // 出局呼叫（按需）
    std::string dial = getenvOr("ITACATI_DIAL", "");
    std::unique_ptr<MyCall> outCall;
    if (!dial.empty() && acc) {
      std::this_thread::sleep_for(std::chrono::milliseconds(800)); // 等待注册
      try {
        outCall = std::make_unique<MyCall>(*acc.get());
        CallOpParam prm(true); // 生成 SDP
        outCall->makeCall(dial, prm);
        std::cout << "[Dial] calling " << dial << std::endl;
      } catch (Error &e) {
        std::cerr << "[Dial] error: " << e.info() << std::endl;
      }
    }

    std::cout << "Running. Press ENTER to quit." << std::endl;
    std::string line; std::getline(std::cin, line);

    if (outCall) { try { CallOpParam bye; outCall->hangup(bye); } catch(...) {} outCall.reset(); }
    if (acc) { acc->shutdown(); acc.reset(); }
    ep.libDestroy();
    std::cout << "Stopped." << std::endl;
    return 0;
  } catch (Error &err) {
    std::cerr << "PJSUA2 error: " << err.info() << std::endl;
    try { ep.libDestroy(); } catch(...) {}
    return 1;
  }
}