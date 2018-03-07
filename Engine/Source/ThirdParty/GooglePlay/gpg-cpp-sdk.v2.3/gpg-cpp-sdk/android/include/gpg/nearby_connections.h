// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/nearby_connections.h
 *
 * @brief API for advertising and discovering nearby endpoints,
 * creating connections, and sending
 *        messages between them.
 */

#ifndef GPG_NEARBY_CONNECTIONS_H_
#define GPG_NEARBY_CONNECTIONS_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <string>
#include <vector>

#include "gpg/common.h"
#include "gpg/endpoint_discovery_listener_helper.h"
#include "gpg/i_endpoint_discovery_listener.h"
#include "gpg/i_message_listener.h"
#include "gpg/message_listener_helper.h"
#include "gpg/nearby_connection_types.h"
#include "gpg/platform_configuration.h"

namespace gpg {

class NearbyConnectionsBuilderImpl;
class NearbyConnectionsImpl;

/**
 * An API used for creating connections and communicating between apps on the
 * same local network.
 */
class GPG_EXPORT NearbyConnections {
 public:
  /**
   * A forward declaration of the Builder type.
   * For more information, see documentation on
   * {@link NearbyConnections::Builder}.
   */
  class Builder;

  NearbyConnections();
  ~NearbyConnections();

  /**
   * Returns the identifier used for this endpoint when the app is communicating
   * with remote endpoints. The identifier has a different value for each
   * <code>NearbyConnections</code> instance.
   */
  std::string GetLocalEndpointId();

  /**
   * Returns the identifier used for this device when the app is communicating
   * with remote endpoints. The identifer has the same value for all clients on
   * this device, and
   * remains stable across reboots of the device.
   */
  std::string GetLocalDeviceId();

  // Methods for endpoints that advertise.

  /**
   * Starts advertising an endpoint for a local app.
   *
   * <code>name</code> can be a name that the app displays to users to identify
   * the endpoint. If you specify an empty string, the device name is used.
   * If specified, <code>app_identifiers</code> specifies how to
   * install or launch this app on different platforms.
   * <code>duration</code> specifies the duration (in milliseconds) for which
   * the advertisement will run, unless the app invokes
   * <code>StopAdvertising()</code> or <code>Stop()</code> before
   * the duration expires. If the value of <code>duration</code> is equal to
   * <code>gpg::Duration::zero()</code>, advertising
   * continues indefinitely until the app calls <code>StopAdvertising()</code>.
   * This function invokes <code>start_advertising_callback</code> when
   * advertising starts or
   * fails; this callback receives the endpoint info on success or an error
   * code on failure.
   * This function invokes <code>request_callback</code> when a remote endpoint
   * requests
   * a connection with the app's endpoint.
   * This function continues advertising the presence of this endpoint until the
   * app calls <code>StopAdvertising</code>, or the duration elapses.
   * If there is already an endpoint being advertised, this call fails.
   */
  void StartAdvertising(std::string const &name,
                        std::vector<AppIdentifier> const &app_identifiers,
                        Duration duration,
                        StartAdvertisingCallback start_advertising_callback,
                        ConnectionRequestCallback request_callback);

  /**
   * Stops advertising the local endpoint. Doing so does NOT cause existing
   * connections to be torn down.
   */
  void StopAdvertising();

  /**
   * Accepts a connection request. Subsequently, the app can send messages to,
   * and receive them from, the specified endpoint. It can do so, using
   * <code>listener</code>, until the app disconnects from the other endpoint.
   * <code>remote_endpoint_id</code> must match the ID of the remote endpoint
   * that requested
   * the connection. <code>ConnectionRequestCallback</code> provides
   * that ID.
   * <code>payload</code> can hold a message to send along with the connection
   * response. <code>listener</code> specifies a listener to be notified of
   * events for this connection.
   */
  void AcceptConnectionRequest(std::string const &remote_endpoint_id,
                               std::vector<uint8_t> const &payload,
                               IMessageListener *listener);

  /**
   * Accepts a connection request. This function only differs from
   * <code>AcceptConnectionRequest</code> in that it uses
   * <code>MessageListenerHelper</code>, rather than
   * <code>IMessageListener</code>.
   */
  void AcceptConnectionRequest(std::string const &remote_endpoint_id,
                               std::vector<uint8_t> const &payload,
                               MessageListenerHelper helper);

  /**
   * Rejects a connection request.
   * <code>remote_endpoint_id</code> must match the ID of the remote endpoint
   * that requested the connection. <code>ConnectionRequestCallback</code>
   * provides that ID.
   */
  void RejectConnectionRequest(std::string const &remote_endpoint_id);

  // Methods for endpoints that discover other endpoints, and request
  // connections to them.

  /**
   * Finds remote endpoints with the specified service
   * ID, and invokes the listener repeatedly as the app finds and loses
   * endpoints. Continues
   * doing so until the app invokes <code>StopDiscovery</code> with the
   * specified service ID.
   * <code>service_id</code> should match the value apps provide when
   * advertising via
   * <code>StartAdvertising</code>.
   * <code>duration</code> specifies the maximum duration (in milliseconds)
   * for discovery to run (it may stop sooner if the app invokes
   * <code>StopDiscovery()</code>). If the value of <code>Duration</code>
   * is equal to <code>gpg::Duration::zero()</code>, discovery continues
   * indefinitely until the app calls
   * <code>StopDiscovery()</code> or <code>Stop()</code>.
   * If there is already a listener
   * registered for finding endpoints for the specified service ID,
   * this call fails.
   */
  void StartDiscovery(std::string const &service_id,
                      Duration duration,
                      IEndpointDiscoveryListener *listener);

  /**
   * Finds remote endpoints with the specified service
   * ID. This function differs from <code>StartDiscovery</code> only in that it
   * uses <code>EndpointDiscoveryListenerHelper</code> instead of
   * <code>IEndpointDiscoveryListener</code>.
   */
  void StartDiscovery(std::string const &service_id,
                      Duration duration,
                      EndpointDiscoveryListenerHelper helper);

  /**
   * Stops finding remote endpoints for a previously specified service ID.
   */
  void StopDiscovery(std::string const &service_id);

  /**
   * Requests that a connection be established with a remote endpoint.
   * <code>name</code> is a name that the app can display to users on the other
   * device to identify this endpoint. If you specify an empty string, the
   * device name is used.
   * <code>remote_endpoint_id</code> is the ID of the remote endpoint to which
   * this app is sending a request to connect.
   * <code>payload</code> can hold a custom message to send along with the
   * connection request. Alternatively, instead of a payload, your app can
   * pass an empty byte vector. This function invokes the specified callback in
   * response to the request. If the operation is successful, it produces a
   * "Connection Accepted" or "Connection Rejected" response. Otherwise, it
   * generates a failure message. In the case of an accepted connection, the app
   * can send messages to the remote endpoint, and the app invokes the specified
   * listener on receipt of a message or disconnection from the remote endpoint.
   */
  void SendConnectionRequest(std::string const &name,
                             std::string const &remote_endpoint_id,
                             std::vector<uint8_t> const &payload,
                             ConnectionResponseCallback callback,
                             IMessageListener *listener);

  /**
   * Requests a connection to a remote endpoint.
   * Differs from <code>SendConnectionRequest</code> only in that it uses
   * <code>MessageListenerHelper</code> instead of
   * <code>IMessageListener</code>.
   */
  void SendConnectionRequest(std::string const &name,
                             std::string const &remote_endpoint_id,
                             std::vector<uint8_t> const &payload,
                             ConnectionResponseCallback callback,
                             MessageListenerHelper helper);

  // Methods used both by endpoints that advertise and by
  // endpoints that discover other instances.

  /**
   * Sends a reliable message to the remote endpoint with the specified
   * ID.
   */
  void SendReliableMessage(std::string const &remote_endpoint_id,
                           std::vector<uint8_t> const &payload);

  /**
   * Sends a reliable message to the remote endpoints with the
   * specified IDs.
   */
  void SendReliableMessage(std::vector<std::string> const &remote_endpoint_ids,
                           std::vector<uint8_t> const &payload);

  /**
   * Sends an unreliable message to the remote endpoint with the
   * specified ID.
   */
  void SendUnreliableMessage(std::string const &remote_endpoint_id,
                             std::vector<uint8_t> const &payload);

  /**
   * Sends an unreliable message to the remote endpoints with the
   * specified IDs.
   */
  void SendUnreliableMessage(
      std::vector<std::string> const &remote_endpoint_ids,
      std::vector<uint8_t> const &payload);

  /**
   * Disconnects from the remote endpoint with the specified ID.
   */
  void Disconnect(std::string const &remote_endpoint_id);

  /**
   * Disconnects from all remote endpoints; stops any advertising or
   * discovery that is taking place. Clears up internal state.
   */
  void Stop();

 private:
  NearbyConnections(std::unique_ptr<NearbyConnectionsBuilderImpl> builder_impl,
                    PlatformConfiguration const &platform);
  NearbyConnections(NearbyConnections const &) = delete;
  NearbyConnections &operator=(NearbyConnections const &) = delete;

  std::shared_ptr<NearbyConnectionsImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_NEARBY_CONNECTIONS_H_
