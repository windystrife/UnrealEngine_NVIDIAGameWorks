// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/real_time_event_listener_helper.h
 * @brief Builds an interface for listening to <code>RealTimeRoom</code> events.
 */

#ifndef GPG_REAL_TIME_EVENT_LISTENER_HELPER_H_
#define GPG_REAL_TIME_EVENT_LISTENER_HELPER_H_

#include <vector>
#include "gpg/multiplayer_participant.h"
#include "gpg/real_time_room.h"

namespace gpg {

class RealTimeEventListenerHelperImpl;

/**
 * Defines a helper which can be used to provide IRealTimeEventListener
 * callbacks to the SDK without defining the full IRealTimeEventListener
 * interface. Callbacks configured on this object will be invoked by the Real-
 * Time multiplayer APIs as described in RealTimeMultiplayerManager. Callbacks
 * not explicitly set will do nothing.
 */
class GPG_EXPORT RealTimeEventListenerHelper {
 public:
  RealTimeEventListenerHelper();

  /**
   * Constructs a RealTimeEventListenerHelper from a
   * <code>shared_ptr</code> to a <code>RealTimeEventListenerHelperImpl</code>.
   * Intended for internal use by the API.
   */
  RealTimeEventListenerHelper(
      std::shared_ptr<RealTimeEventListenerHelperImpl> impl);

  /**
   * <code>OnRoomStatusChangedCallback</code> is called when a
   * <code>RealTimeRoom</code> object's <code>Status()</code> method returns an
   * update.
   */
  typedef std::function<void(RealTimeRoom const &)> OnRoomStatusChangedCallback;

  /**
   * Set the OnRoomStatusChangedCallback.
   */
  RealTimeEventListenerHelper &SetOnRoomStatusChangedCallback(
      OnRoomStatusChangedCallback callback);

  /**
   * <code>OnConnectedSetChangedCallback</code> is called when a
   * <code>MultiplayerParticipant</code> object connects or disconnects from the
   * room's
   * connected set.
   */
  typedef std::function<void(RealTimeRoom const &)>
      OnRoomConnectedSetChangedCallback;

  /**
   * Set the OnRoomConnectedSetChangedCallback.
   */
  RealTimeEventListenerHelper &SetOnRoomConnectedSetChangedCallback(
      OnRoomConnectedSetChangedCallback callback);

  /**
   * <code>OnP2PConnectedCallback</code> is called when a
   * <code>MultiplayerParticipant</code> object connects directly to the local
   * player.
   */
  typedef std::function<void(RealTimeRoom const &,
                             MultiplayerParticipant const &)>
      OnP2PConnectedCallback;

  /**
   * Set the OnP2PConnectedCallback.
   */
  RealTimeEventListenerHelper &SetOnP2PConnectedCallback(
      OnP2PConnectedCallback callback);

  /**
   * <code>OnP2PDisconnectedCallback</code> is called when a
   * <code>MultiplayerParticipant</code> object disconnects directly from the
   * local
   * player.
   */
  typedef std::function<void(RealTimeRoom const &,
                             MultiplayerParticipant const &)>
      OnP2PDisconnectedCallback;

  /**
   * Set the OnP2PDisconnectedCallback.
   */
  RealTimeEventListenerHelper &SetOnP2PDisconnectedCallback(
      OnP2PDisconnectedCallback callback);

  /**
   * <code>OnParticipantStatusChangedCallback</code> is called when a
   * <code>MultiplayerParticipant</code> object disconnects directly from the
   * local
   * player.
   */
  typedef std::function<void(RealTimeRoom const &,
                             MultiplayerParticipant const &)>
      OnParticipantStatusChangedCallback;

  /**
   * Set the OnParticipantStatusChangedCallback.
   */
  RealTimeEventListenerHelper &SetOnParticipantStatusChangedCallback(
      OnParticipantStatusChangedCallback callback);

  /**
   * <code>OnDataReceivedCallback</code> is called whenever data is received
   * from
   * another <code>MultiplayerParticipant</code>.
   */
  typedef std::function<void(RealTimeRoom const &room,
                             MultiplayerParticipant const &from_participant,
                             std::vector<uint8_t> data,
                             bool is_reliable)>
      OnDataReceivedCallback;

  /**
   * Set the OnDataReceivedCallback.
   */
  RealTimeEventListenerHelper &SetOnDataReceivedCallback(
      OnDataReceivedCallback callback);

 private:
  friend class RealTimeEventListenerHelperImpl;
  std::shared_ptr<RealTimeEventListenerHelperImpl> impl_;
};

}  // namespace gpg

#endif  // GPG_REAL_TIME_EVENT_LISTENER_HELPER_H_
