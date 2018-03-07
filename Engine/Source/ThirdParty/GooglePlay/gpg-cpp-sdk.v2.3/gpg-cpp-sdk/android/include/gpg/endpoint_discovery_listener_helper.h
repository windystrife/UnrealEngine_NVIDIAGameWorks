// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/endpoint_discovery_listener_helper.h
 * @brief Builds an interface for listening for nearby endpoints that have been
 * discovered.
 */

#ifndef GPG_ENDPOINT_DISCOVERY_LISTENER_HELPER_H_
#define GPG_ENDPOINT_DISCOVERY_LISTENER_HELPER_H_

#include <functional>
#include <string>

#include "gpg/common.h"
#include "gpg/nearby_connection_types.h"

namespace gpg {

class EndpointDiscoveryListenerHelperImpl;

/**
 * Defines a helper which can be used to provide IEndpointDiscoveryListener
 * callbacks to the SDK without defining the full IEndpointDiscoveryListener
 * interface. Callbacks configured on this object will be invoked
 * as described in the nearby connections API. Callbacks
 * not explicitly set will do nothing.
 */
class GPG_EXPORT EndpointDiscoveryListenerHelper {
 public:
  EndpointDiscoveryListenerHelper();

  /**
   * Constructs a EndpointDiscoveryListenerHelper from a
   * <code>shared_ptr</code> to a
   * <code>EndpointDiscoveryListenerHelperImpl</code>. Intended for internal use
   * by the API.
   */
  EndpointDiscoveryListenerHelper(
      std::shared_ptr<EndpointDiscoveryListenerHelperImpl> impl);

  /**
   * <code>OnEndpointFoundCallback</code> is called when a
   * remote endpoint is found.
   * <code>client_id</code> is the ID of the NearbyConnections instance that
   * discovered the endpoint.
   * <code>endpoint_details</code> contains the details of the discovered
   * remote endpoint.
   */
  typedef std::function<void(int64_t client_id,
                             EndpointDetails const &endpoint_details)>
      OnEndpointFoundCallback;

  /**
   * Set the OnEndpointFoundCallback.
   */
  EndpointDiscoveryListenerHelper &SetOnEndpointFoundCallback(
      OnEndpointFoundCallback callback);

  /**
   * <code>OnEndpointLostCallback</code> is called when a
   * remote endpoint is no longer discoverable.
   */
  typedef std::function<void(int64_t client_id,
                             std::string const &remote_endpoint_id)>
      OnEndpointLostCallback;

  /**
   * Set the OnEndpointLostCallback.
   */
  EndpointDiscoveryListenerHelper &SetOnEndpointLostCallback(
      OnEndpointLostCallback callback);

 private:
  friend class EndpointDiscoveryListenerHelperImpl;

  std::shared_ptr<EndpointDiscoveryListenerHelperImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_ENDPOINT_DISCOVERY_LISTENER_HELPER_H_
