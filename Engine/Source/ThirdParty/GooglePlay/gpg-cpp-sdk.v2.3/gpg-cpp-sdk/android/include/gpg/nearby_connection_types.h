// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/nearby_connection_types.h
 *
 *
 */

#ifndef GPG_NEARBY_CONNECTION_TYPES_H_
#define GPG_NEARBY_CONNECTION_TYPES_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gpg {

/**
 * An identifier for an application.
 */
struct GPG_EXPORT AppIdentifier {
  /**
   * The identifier string that the app uses to find or install an application
   * on the platform. For Android, this string is a package name.
   */
  std::string identifier;
};

/**
 * The ID and name of an instance registered on this device.
 * @ingroup ResponseType
 */
struct GPG_EXPORT StartAdvertisingResult {
  /**
   * Status code values returned in the <code>status</code> field.
   */
  enum class StatusCode {
    SUCCESS = 1,
    ERROR_INTERNAL = -1,
    ERROR_NETWORK_NOT_CONNECTED = -2,
    ERROR_ALREADY_ADVERTISING = -3,
  };

  /**
   * The status code indicating whether advertising succeeded.
   */
  StatusCode status;

  /**
   * The human-readable name for the local endpoint being advertised
   * (after resolving any name collisions that may have occurred).
   */
  std::string local_endpoint_name;
};

/**
 * Details about a remote endpoint that the app has discovered.
 * @ingroup ResponseType
 */
struct GPG_EXPORT EndpointDetails {
  /**
   * The ID for the remote endpoint.
   */
  std::string endpoint_id;

  /**
   * The ID for the remote device.
   */
  std::string device_id;

  /**
   * The human-readable name of the remote endpoint.
   */
  std::string name;

  /**
   * The ID of the service running on the remote endpoint.
   */
  std::string service_id;
};

/**
 * A request to establish a connection.
 */
struct GPG_EXPORT ConnectionRequest {
  /**
   * The ID of the remote endpoint that is requesting a connection.
   */
  std::string remote_endpoint_id;

  /**
   * The ID of the remote device that is requesting a connection.
   */
  std::string remote_device_id;

  /**
   * The name of the instance that is requesting a connection.
   */
  std::string remote_endpoint_name;

  /**
   * A custom message sent with the connection request.
   */
  std::vector<uint8_t> payload;
};

/**
 * A response to a connection request.
 */
struct GPG_EXPORT ConnectionResponse {
  /**
   * Status code values returned in the <code>status</code> field.
   */
  enum class StatusCode {
    ACCEPTED = 1,
    REJECTED = 2,
    ERROR_INTERNAL = -1,
    ERROR_NETWORK_NOT_CONNECTED = -2,
    ERROR_ENDPOINT_ALREADY_CONNECTED = -3,
    ERROR_ENDPOINT_NOT_CONNECTED = -4,
  };

  /**
   * The ID of the remote endpoint to which a connection request was sent.
   */
  std::string remote_endpoint_id;

  /**
   * The status code indicating whether the connection was accepted.
   */
  StatusCode status;

  /**
   * A custom message that the app can send with the connection response.
   */
  std::vector<uint8_t> payload;
};

/**
 * Defines a callback type that receives a {@link ConnectionRequest} when a
 * remote endpoint attempts to connect to the app's own endpoint.
 * <code>client_id</code> is the ID of the <code>NearbyConnections</code>
 * instance that received this request. <code>request</code> contains the
 * details of the connection request.
 * @ingroup Callbacks
 */
typedef std::function<void(int64_t client_id, ConnectionRequest const &request)>
    ConnectionRequestCallback;

/**
 * Defines a callback type that receives a {@link StartAdvertisingResult} when a
 * local endpoint advertising attempt is complete; its success field indicates
 * whether advertising started successfully. <code>client_id</code> is the ID of
 * the <code>NearbyConnections</code> instance that tried to start advertising.
 * <code>result</code> contains the results of that advertisement.
 * @ingroup Callbacks
 */
typedef std::function<void(int64_t client_id,
                           StartAdvertisingResult const &result)>
    StartAdvertisingCallback;

/**
 * Defines a callback type that receives a {@link
 * ConnectionResponse} when a response arrives after an
 * attempt to establish a connection to a remote endpoint.
 * <code>client_id</code> is the ID of the <code>NearbyConnections</code>
 * instance that sent the connection request. <code>response</code> contains the
 * details of the response.
 */
typedef std::function<void(int64_t client_id,
                           ConnectionResponse const &response)>
    ConnectionResponseCallback;

}  // namespace gpg

#endif  // GPG_NEARBY_CONNECTION_TYPES_H_
