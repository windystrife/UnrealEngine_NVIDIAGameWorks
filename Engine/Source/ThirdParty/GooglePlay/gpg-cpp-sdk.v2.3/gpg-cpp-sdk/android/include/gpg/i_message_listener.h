// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/i_message_listener.h
 * @brief An interface for listening for messages from remote endpoints.
 */

#ifndef GPG_I_MESSAGE_LISTENER_H_
#define GPG_I_MESSAGE_LISTENER_H_

#include <string>
#include <vector>
#include "gpg/common.h"

namespace gpg {

/**
 * Defines an interface which can be delivered messages from remote endpoints.
 */
class GPG_EXPORT IMessageListener {
 public:
  virtual ~IMessageListener() {}

  /**
   * Invoked when a message is received from a remote endpoint.
   */
  virtual void OnMessageReceived(int64_t client_id,
                                 std::string const &remote_endpoint_id,
                                 std::vector<uint8_t> const &payload,
                                 bool is_reliable) = 0;

  /**
   * Invoked when a remote endpoint is disconnected.
   */
  virtual void OnDisconnected(int64_t client_id,
                              std::string const &remote_endpoint_id) = 0;
};

}  // namespace gpg

#endif  // GPG_I_MESSAGE_LISTENER_H_
