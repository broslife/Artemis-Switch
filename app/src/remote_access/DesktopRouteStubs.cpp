//
//  DesktopRouteStubs.cpp
//  Artemis
//
//  Remote-access routing exists only on the Switch (VPN stacks). Other
//  platforms still link GameStreamClient, so these answer "no route needed":
//  every address is dialed directly, as on a LAN.
//

#include "RemoteRouting.hpp"

bool RemoteAccessManager::prepareRouteForStreaming(const std::string&,
                                                   const std::string&,
                                                   const std::string&) {
    return true;
}

void RemoteAccessManager::deactivateRoute(const std::string&,
                                          const std::string&,
                                          const std::string&) {}

namespace artemis::remote {

RemoteRouteLease acquireRouteFor(const std::string&) {
    return RemoteRouteLease();
}

std::string connectAddressFor(const RemoteRouteLease&,
                              const std::string& address) {
    return address;
}

std::string routeRefusalReason(const RemoteRouteLease&) {
    return "Remote routing is not available on this platform";
}

void logConnectionAttempt(const RemoteRouteLease&, const std::string&,
                          const std::string&) {}

void logConnectionResult(const RemoteRouteLease&, const std::string&, bool,
                         const std::string&) {}

void logProxiedPairingResult(const std::string&, bool, const std::string&) {}

} // namespace artemis::remote
