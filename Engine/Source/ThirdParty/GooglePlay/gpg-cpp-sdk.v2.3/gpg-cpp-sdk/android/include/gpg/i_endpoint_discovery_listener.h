// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/i_endpoint_discovery_listener.h
 * @brief An interface for endpoint discovery events.
 */

#ifndef GPG_I_ENDPOINT_DISCOVERY_LISTENER_H_
#define GPG_I_ENDPOINT_DISCOVERY_LISTENER_H_

#include <string>
#include "gpg/common.h"
#include "gpg/nearby_connection_types.h"

namespace gpg {

/**
 * Defines an interface which can be delivered events relating
 * to remote endpoint discovery.
 */
class GPG_EXPORT IEndpointDiscoveryListener {
 public:
  virtual ~IEndpointDiscoveryListener() {}

  /**
   * Invoked when a remote endpoint is found; will be invoked once for each
   * endpoint discovered. Note that this method may be invoked repeatedly in
   * short succession; you may wish to delay the update to the UI to reflect the
   * new endpoint for some short time period after the method is invoked.
   */
  virtual void OnEndpointFound(int64_t client_id,
                               EndpointDetails const &endpoint_details) = 0;

  /**
   * Invoked when a remote endpoint is no longer discoverable;
   * will only be called with IDs that previously were passed to
   * <code>OnEndpointFound</code>.
   * Note that this method may be invoked repeatedly in short succession; you
   * may with to delay the update to the UI to reflect the endpoint being gone
   * for some short time period after the method is invoked.
   */
  virtual void OnEndpointLost(int64_t client_id,
                              std::string const &remote_endpoint_id) = 0;
};

}  // namespace gpg

#endif  // GPG_I_ENDPOINT_DISCOVERY_LISTENER_H_
