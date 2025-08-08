#include <pjsua2.hpp>
#include <iostream>
#include <memory>
#include <cstdlib>

using namespace pj;

static std::string getenvOr(const char* k, const char* defv) {
  const char* v = std::getenv(k);
  return v ? std::string(v) : std::string(defv);
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

int main() {
  Endpoint ep;
  try {
    ep.libCreate();

    EpConfig epcfg;
    epcfg.uaConfig.maxCalls = 16;
    epcfg.logConfig.level = 4;
    epcfg.logConfig.consoleLevel = 4;
    epcfg.medConfig.sndAutoCloseTime = 0;
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
    std::cout << "PJSUA2 started." << std::endl;

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

    std::cout << "Running. Press ENTER to quit." << std::endl;
    std::string line; std::getline(std::cin, line);

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