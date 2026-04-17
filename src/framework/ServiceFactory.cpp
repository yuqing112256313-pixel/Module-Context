#include "framework/ServiceFactory.h"

#include "foundation/base/ErrorCode.h"

namespace module_context {
namespace framework {

ServiceFactory::ServiceFactory()
    : services_by_key_() {
}

ServiceFactory::~ServiceFactory() {
}

void ServiceFactory::Register(
    const std::string& service_key,
    const std::string& name,
    IModule* provider) {
    if (service_key.empty() || name.empty() || provider == NULL) {
        return;
    }

    services_by_key_[service_key][name] = provider;
}

foundation::base::Result<IModule*> ServiceFactory::Lookup(
    const std::string& service_key,
    const std::string& name) const {
    if (service_key.empty() || name.empty()) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kInvalidArgument,
            "ServiceFactory::Lookup failed: service key or name is empty");
    }

    ServicesByKey::const_iterator by_key = services_by_key_.find(service_key);
    if (by_key == services_by_key_.end()) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kNotFound,
            "ServiceFactory::Lookup failed: service key '" + service_key +
                "' has no provider named '" + name + "'");
    }

    ServiceMap::const_iterator by_name = by_key->second.find(name);
    if (by_name == by_key->second.end()) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kNotFound,
            "ServiceFactory::Lookup failed: service key '" + service_key +
                "' has no provider named '" + name + "'");
    }

    if (by_name->second == NULL) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kInvalidState,
            "ServiceFactory::Lookup failed: service '" + name +
                "' is not available");
    }

    return foundation::base::Result<IModule*>(by_name->second);
}

foundation::base::Result<IModule*> ServiceFactory::LookupUnique(
    const std::string& service_key) const {
    if (service_key.empty()) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kInvalidArgument,
            "ServiceFactory::LookupUnique failed: service key is empty");
    }

    ServicesByKey::const_iterator by_key = services_by_key_.find(service_key);
    if (by_key == services_by_key_.end() || by_key->second.empty()) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kNotFound,
            "ServiceFactory::LookupUnique failed: service was not found");
    }

    if (by_key->second.size() != 1) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kInvalidState,
            "ServiceFactory::LookupUnique failed: multiple services are registered");
    }

    IModule* provider = by_key->second.begin()->second;
    if (provider == NULL) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kInvalidState,
            "ServiceFactory::LookupUnique failed: service is not available");
    }

    return foundation::base::Result<IModule*>(provider);
}

void ServiceFactory::Clear() {
    services_by_key_.clear();
}

}  // namespace framework
}  // namespace module_context
