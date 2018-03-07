// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/message_listener_helper.h
 * @brief Builds an interface for listening to nearby connection messages.
 */

#ifndef GPG_MESSAGE_LISTENER_HELPER_H_
#define GPG_MESSAGE_LISTENER_HELPER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gpg/common.h"

namespace gpg {

class MessageListenerHelperImpl;

/**
 * Defines a helper which can be used to provide IMessageListener
 * callbacks to the SDK without defining the full IMessageListener
 * interface. Callbacks configured on this object will be invoked
 * as described in the nearby connections API. Callbacks
 * not explicitly set will do nothing.
 */
class GPG_EXPORT MessageListenerHelper {
 public:
  MessageListenerHelper();

  /**
   * Constructs a MessageListenerHelper from a
   * <code>shared_ptr</code> to a <code>MessageListenerHelperImpl</code>.
   * Intended for internal use by the API.
   */
  explicit MessageListenerHelper(
      std::shared_ptr<MessageListenerHelperImpl> impl);

  /**
   * <code>OnMessageReceivedCallback</code> is called when a
   * message is received from a remote endpoint.
   * <code>client_id</code> is the ID of the NearbyConnections instance that
   * received this message.
   * <code>remote_endpoint_id</code> is the ID of the remote endpoint that
   * sent the message.
   * <code>payload</code> contains the bytes of the message.
   * <code>is_reliable</code> is true if the message was sent reliably, false
   * otherwise.
   */
  typedef std::function<void(int64_t client_id,
                             std::string const &remote_endpoint_id,
                             std::vector<uint8_t> const &payload,
                             bool is_reliable)>
      OnMessageReceivedCallback;

  /**
   * Set the OnMessageReceivedCallback.
   */
  MessageListenerHelper &SetOnMessageReceivedCallback(
      OnMessageReceivedCallback callback);

  /**
   * <code>OnDisconnectedCallback</code> is called when a
   * remote endpoint disconnects.
   * <code>client_id</code> is the ID of the NearbyConnections instance that
   * received the disconnect message.
   * <code>remote_endpoint_id</code> is the ID of the remote endpoint that
   * disconnected.
   */
  typedef std::function<void(int64_t client_id,
                             std::string const &remote_endpoint_id)>
      OnDisconnectedCallback;

  /**
   * Set the OnDisconnectedCallback.
   */
  MessageListenerHelper &SetOnDisconnectedCallback(
      OnDisconnectedCallback callback);

 private:
  friend class MessageListenerHelperImpl;

  std::shared_ptr<MessageListenerHelperImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_MESSAGE_LISTENER_HELPER_H_
