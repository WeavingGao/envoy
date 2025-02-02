#include "source/common/network/dns_resolver/dns_factory.h"

namespace Envoy {
namespace Network {

// Create a default c-ares DNS resolver typed config.
void makeDefaultCaresDnsResolverConfig(
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  envoy::extensions::network::dns_resolver::cares::v3::CaresDnsResolverConfig cares;
  typed_dns_resolver_config.mutable_typed_config()->PackFrom(cares);
  typed_dns_resolver_config.set_name(std::string(CaresDnsResolver));
}

// Create a default apple DNS resolver typed config.
void makeDefaultAppleDnsResolverConfig(
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  envoy::extensions::network::dns_resolver::apple::v3::AppleDnsResolverConfig apple;
  typed_dns_resolver_config.mutable_typed_config()->PackFrom(apple);
  typed_dns_resolver_config.set_name(std::string(AppleDnsResolver));
}

// Create a default DNS resolver typed config based on build system and configuration.
void makeDefaultDnsResolverConfig(
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  // If use apple API for DNS lookups, create an AppleDnsResolverConfig typed config.
  if (checkUseAppleApiForDnsLookups(typed_dns_resolver_config)) {
    return;
  }
  // Otherwise, create a CaresDnsResolverConfig typed config.
  makeDefaultCaresDnsResolverConfig(typed_dns_resolver_config);
}

// If it is MacOS and the run time flag: envoy.restart_features.use_apple_api_for_dns_lookups
// is enabled, create an AppleDnsResolverConfig typed config.
bool checkUseAppleApiForDnsLookups(
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  if (Runtime::runtimeFeatureEnabled("envoy.restart_features.use_apple_api_for_dns_lookups")) {
    if (Config::Utility::getAndCheckFactoryByName<Network::DnsResolverFactory>(
            std::string(AppleDnsResolver), true) != nullptr) {
      makeDefaultAppleDnsResolverConfig(typed_dns_resolver_config);
      ENVOY_LOG_MISC(debug, "create Apple DNS resolver type: {} in MacOS.",
                     typed_dns_resolver_config.name());
      return true;
    }
#ifdef __APPLE__
    RELEASE_ASSERT(false,
                   "In MacOS, if run-time flag 'use_apple_api_for_dns_lookups' is enabled, "
                   "but the envoy.network.dns_resolver.apple extension is not included in Envoy "
                   "build file. This is wrong. Abort Envoy.");
#endif
  }
  return false;
}

// Overloading the template function for DnsFilterConfig type, which doesn't need to copy anything.
void handleLegacyDnsResolverData(
    const envoy::extensions::filters::udp::dns_filter::v3::DnsFilterConfig::ClientContextConfig&,
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  makeDefaultCaresDnsResolverConfig(typed_dns_resolver_config);
}

// Overloading the template function for Cluster config type, which need to copy
// both use_tcp_for_dns_lookups and dns_resolvers.
void handleLegacyDnsResolverData(
    const envoy::config::cluster::v3::Cluster& config,
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  envoy::extensions::network::dns_resolver::cares::v3::CaresDnsResolverConfig cares;
  cares.mutable_dns_resolver_options()->set_use_tcp_for_dns_lookups(
      config.use_tcp_for_dns_lookups());
  if (!config.dns_resolvers().empty()) {
    cares.mutable_resolvers()->MergeFrom(config.dns_resolvers());
  }
  typed_dns_resolver_config.mutable_typed_config()->PackFrom(cares);
  typed_dns_resolver_config.set_name(std::string(CaresDnsResolver));
}

// Create the DNS resolver factory from the typed config. This is the underline
// function which performs the registry lookup based on typed config.
Network::DnsResolverFactory& createDnsResolverFactoryFromTypedConfig(
    const envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  ENVOY_LOG_MISC(debug, "create DNS resolver type: {}", typed_dns_resolver_config.name());
  return Config::Utility::getAndCheckFactory<Network::DnsResolverFactory>(
      typed_dns_resolver_config);
}

// Create the default DNS resolver factory. apple for MacOS or c-ares for all others.
// The default registry lookup will always succeed, thus no exception throwing.
// This function can be called in main or worker threads.
Network::DnsResolverFactory& createDefaultDnsResolverFactory(
    envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
  Network::makeDefaultDnsResolverConfig(typed_dns_resolver_config);
  return createDnsResolverFactoryFromTypedConfig(typed_dns_resolver_config);
}

} // namespace Network
} // namespace Envoy
