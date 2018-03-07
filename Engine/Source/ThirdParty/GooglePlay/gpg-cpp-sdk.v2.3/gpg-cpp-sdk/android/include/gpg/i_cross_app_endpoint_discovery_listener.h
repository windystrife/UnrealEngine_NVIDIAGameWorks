// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/i_cross_app_endpoint_discovery_listener.h
 * @brief An interface for cross-app endpoint discovery events.
 */

#ifndef GPG_I_CROSS_APP_ENDPOINT_DISCOVERY_LISTENER_H_
#define GPG_I_CROSS_APP_ENDPOINT_DISCOVERY_LISTENER_H_

#include <string>
#include "gpg/common.h"
#include "gpg/nearby_connection_types.h"

namespace gpg {

/**
 * Defines an interface which can be delivered events relating
 * to cross-app remote endpoint discovery.
 */
class ICrossAppEndpointDiscoveryListener {
 public:
  virtual ~ICrossAppEndpointDiscoveryListener() {}

  /**
   * Invoked when a remote endpoint is found; will be invoked once for each
   * endpoint discovered. Note that this method may be invoked repeatedly in
   * short succession; you may wish to delay the update to the UI to reflect the
   * new endpoint for some short time period after the method is invoked.
   */
  virtual void OnCrossAppEndpointFound(
      int64_t client_id,
      EndpointDetails const &endpoint_details,
      std::vector<AppIdentifier> const &app_identifiers) = 0;

  /**
   * Invoked when a remote endpoint is no longer discoverable;
   * will only be called with IDs that previously were passed to
   * <code>OnEndpointFound</code>.
   * Note that this method may be invoked repeatedly in short succession; you
   * may with to delay the update to the UI to reflect the endpoint being gone
   * for some short time period after the method is invoked.
   */
  virtual void OnCrossAppEndpointLost(int64_t client_id,
                                      std::string const &instance_id) = 0;
};

}  // namespace gpg

#endif  // GPG_I_CROSS_APP_ENDPOINT_DISCOVERY_LISTENER_H_
