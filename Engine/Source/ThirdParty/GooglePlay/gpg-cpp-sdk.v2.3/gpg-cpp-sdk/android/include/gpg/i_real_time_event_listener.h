// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/i_real_time_event_listener.h
 * @brief An interface for listening to <code>RealTimeRoom</code> events.
 */

#ifndef GPG_I_REAL_TIME_EVENT_LISTENER_H_
#define GPG_I_REAL_TIME_EVENT_LISTENER_H_

#include <vector>
#include "gpg/multiplayer_participant.h"
#include "gpg/real_time_room.h"

namespace gpg {

/**
 * Defines an interface that can deliver events relating to real-time
 * multiplayer.
 */
class GPG_EXPORT IRealTimeEventListener {
 public:
  virtual ~IRealTimeEventListener() {}

  /**
   * <code>OnRoomStatusChanged</code> is called when a
   * <code>RealTimeRoom</code> object's <code>Status()</code> method returns an
   * update.
   *
   * @param room The room whose status changed.
   */
  virtual void OnRoomStatusChanged(RealTimeRoom const &room) = 0;

  /**
   * <code>OnConnectedSetChanged</code> is called when a
   * <code>MultiplayerParticipant</code> object connects or disconnects from the
   * room's connected set.
   *
   * @param room The room whose connected set changed.
   */
  virtual void OnConnectedSetChanged(RealTimeRoom const &room) = 0;

  /**
   * <code>OnP2PConnected</code> is called when a
   * <code>MultiplayerParticipant</code> object connects directly to the local
   * player.
   *
   * @param room The room in which the <code>participant</code> is located.
   * @param participant The participant that connected.
   */
  virtual void OnP2PConnected(RealTimeRoom const &room,
                              MultiplayerParticipant const &participant) = 0;

  /**
   * <code>OnP2PDisconnected</code> is called when a
   * <code>MultiplayerParticipant</code> object disconnects directly from the
   * local player.
   *
   * @param room The room in which the participant is located.
   * @param participant The participant that disconnected.
   */
  virtual void OnP2PDisconnected(RealTimeRoom const &room,
                                 MultiplayerParticipant const &participant) = 0;

  /**
   * <code>OnParticipantStatusChanged</code> is called when a
   * <code>MultiplayerParticipant</code> object's <code>Status()</code>
   * method returns an update.
   *
   * @param room The room which <code>participant</code> is in.
   * @param participant The participant whose status changed.
   */
  virtual void OnParticipantStatusChanged(
      RealTimeRoom const &room, MultiplayerParticipant const &participant) = 0;

  /**
   * <code>OnDataReceived</code> is called whenever data is received from
   * another <code>MultiplayerParticipant</code>.
   *
   * @param room The room in which <code>from_participant</code> is located.
   * @param from_participant The participant who sent the data.
   * @param data The data which was recieved.
   * @param is_reliable Whether the data was sent using the unreliable or
   *                    reliable mechanism.
   */
  virtual void OnDataReceived(RealTimeRoom const &room,
                              MultiplayerParticipant const &from_participant,
                              std::vector<uint8_t> data,
                              bool is_reliable) = 0;
};

}  // namespace gpg

#endif  // GPG_I_REAL_TIME_EVENT_LISTENER_H_
