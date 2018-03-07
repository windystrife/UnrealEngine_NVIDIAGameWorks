// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/real_time_multiplayer_manager.h
 * @brief Entry points for Play Games RealTime Multiplayer functionality.
 */

#ifndef GPG_REAL_TIME_MULTIPLAYER_MANAGER_H_
#define GPG_REAL_TIME_MULTIPLAYER_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <vector>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/i_real_time_event_listener.h"
#include "gpg/multiplayer_invitation.h"
#include "gpg/player.h"
#include "gpg/real_time_event_listener_helper.h"
#include "gpg/real_time_room.h"
#include "gpg/real_time_room_config.h"
#include "gpg/turn_based_multiplayer_manager.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Fetches, modifies, handles messaging for, and creates
 * <code>RealTimeRoom</code> objects.
 * @ingroup Managers
 */
class GPG_EXPORT RealTimeMultiplayerManager {
 public:
  /**
   * <code>Data</code> and <code>ResponseStatus</code> for a specific
   * <code>RealTimeRoom</code> object. The room value is only valid if
   * <code>IsSuccess()</code> returns true for <code>ResponseStatus</code>.
   *
   * @ingroup ResponseType
   */
  struct RealTimeRoomResponse {
    /**
     * The status of the operation that generated this
     * response.
     */
    MultiplayerStatus status;

    /**
     * The <code>RealTimeRoom</code> object for this response.
     * <code>Valid()</code> only returns true for the room if
     * <code>IsSuccess()</code> returns true for <code>status</code>.
     */
    RealTimeRoom room;
  };

  /**
   * Defines a callback that can be used to receive a
   * <code>RealTimeRoomResponse</code> struct from one of the turn-based
   * multiplayer operations.
   */
  typedef std::function<void(RealTimeRoomResponse const &)>
      RealTimeRoomCallback;

  /**
   * Asynchronously creates a <code>RealTimeRoom</code> object using the
   * provided <code>RealTimeRoomConfig</code> class. If creation is successful,
   * this function returns the <code>RealTimeRoom</code> object via the provided
   * <code>RealTimeRoomCallback</code>.
   */
  void CreateRealTimeRoom(gpg::RealTimeRoomConfig const &config,
                          IRealTimeEventListener *listener,
                          RealTimeRoomCallback callback);

  /**
   * Blocking version of {@link CreateRealTimeRoom}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  RealTimeRoomResponse CreateRealTimeRoomBlocking(
      Timeout timeout,
      RealTimeRoomConfig const &config,
      IRealTimeEventListener *listener);

  /**
   * Overload of {@link CreateRealTimeRoomBlocking}, which uses a default
   * timeout of 10 years.
   */
  RealTimeRoomResponse CreateRealTimeRoomBlocking(
      RealTimeRoomConfig const &config, IRealTimeEventListener *listener);

  /**
   * Asynchronously creates a <code>RealTimeRoom</code> object using the
   * provided <code>RealTimeRoomConfig</code> class. If creation is successful,
   * this function returns the <code>RealTimeRoom</code> object via the provided
   * <code>RealTimeRoomCallback</code>.
   */
  void CreateRealTimeRoom(gpg::RealTimeRoomConfig const &config,
                          RealTimeEventListenerHelper helper,
                          RealTimeRoomCallback callback);

  /**
   * Blocking version of {@link CreateRealTimeRoom}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  RealTimeRoomResponse CreateRealTimeRoomBlocking(
      Timeout timeout,
      RealTimeRoomConfig const &config,
      RealTimeEventListenerHelper helper);

  /**
   * Overload of {@link CreateRealTimeRoomBlocking}, which uses a default
   * timeout of 10 years.
   */
  RealTimeRoomResponse CreateRealTimeRoomBlocking(
      RealTimeRoomConfig const &config, RealTimeEventListenerHelper helper);

  /**
   * Asynchronously accepts a <code>MultiplayerInvitation</code>, and returns
   * the result via a <code>RealTimeRoomCallback</code>.
   */
  void AcceptInvitation(MultiplayerInvitation const &invitation,
                        IRealTimeEventListener *listener,
                        RealTimeRoomCallback callback);

  /**
   * Blocking version of {@link AcceptInvitation}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  RealTimeRoomResponse AcceptInvitationBlocking(
      Timeout timeout,
      MultiplayerInvitation const &invitation,
      IRealTimeEventListener *listener);

  /**
   * Overload of {@link AcceptInvitationBlocking}, which uses a default timeout
   * of 10 years.
   */
  RealTimeRoomResponse AcceptInvitationBlocking(
      MultiplayerInvitation const &invitation,
      IRealTimeEventListener *listener);

  /**
   * Asynchronously accepts a <code>MultiplayerInvitation</code>, and returns
   * the result via a <code>RealTimeRoomCallback</code>.
   */
  void AcceptInvitation(MultiplayerInvitation const &invitation,
                        RealTimeEventListenerHelper helper,
                        RealTimeRoomCallback callback);

  /**
   * Blocking version of {@link AcceptInvitation}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  RealTimeRoomResponse AcceptInvitationBlocking(
      Timeout timeout,
      MultiplayerInvitation const &invitation,
      RealTimeEventListenerHelper helper);

  /**
   * Overload of {@link AcceptInvitationBlocking}, which uses a default timeout
   * of 10 years.
   */
  RealTimeRoomResponse AcceptInvitationBlocking(
      MultiplayerInvitation const &invitation,
      RealTimeEventListenerHelper helper);

  /**
   * Declines a <code>MultiplayerInvitation</code> to a
   * <code>RealTimeRoom</code>. Doing so cancels the room for the other
   * participants, and removes the room from the local player's device.
   */
  void DeclineInvitation(MultiplayerInvitation const &invitation);

  /**
   * Dismisses a <code>MultiplayerInvitation</code> to a
   * <code>RealTimeRoom</code>. This does not change the visible state of the
   * <code>RealTimeRoom</code> for the other participants, but removes it
   * from the local player's device.
   */
  void DismissInvitation(MultiplayerInvitation const &invitation);

  /**
   * Defines a callback that can receive a <code>ResponseStatus</code>
   * from <code>LeaveRoom</code>.
   */
  typedef std::function<void(ResponseStatus const &)> LeaveRoomCallback;

  /**
   * Leaves a <code>RealTimeRoom</code>. You should not create a new room or
   * attempt to join another room until this operation has completed. The result
   * of this operation is returned via a <code>LeaveRoomCallback</code>.
   */
  void LeaveRoom(RealTimeRoom const &room, LeaveRoomCallback callback);

  /**
   * Blocking version of {@link LeaveRoom}. Allows the caller to specify a
   * timeout in ms. After the specified time elapses, the function returns
   * <code>ERROR_TIMEOUT</code>.
   */
  ResponseStatus LeaveRoomBlocking(Timeout timeout, RealTimeRoom const &room);

  /**
   * Overload of {@link LeaveRoomBlocking}, which uses a default timeout
   * of 10 years.
   */
  ResponseStatus LeaveRoomBlocking(RealTimeRoom const &room);

  /**
   * Defines a callback that can receive a <code>ResponseStatus</code>
   * from <code>SendReliableMessage</code>.
   */
  typedef std::function<void(MultiplayerStatus const &)>
      SendReliableMessageCallback;

  /**
   * Sends a message to the specified <code>MultiplayerParticipant</code>. Uses
   * a reliable method to send the message. This method of sending data may take
   * longer than sending a message unreliably. The result of the send is
   * reported via the provided callback.
   */
  void SendReliableMessage(RealTimeRoom const &room,
                           MultiplayerParticipant const &participant,
                           std::vector<uint8_t> data,
                           SendReliableMessageCallback callback);

  /**
   * Blocking version of {@link SendReliableMessage}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  MultiplayerStatus SendReliableMessageBlocking(
      Timeout timeout,
      RealTimeRoom const &room,
      MultiplayerParticipant const &participant,
      std::vector<uint8_t> data);

  /**
   * Overload of {@link SendReliableMessageBlocking}, which uses a default
   * timeout of 10 years.
   */
  MultiplayerStatus SendReliableMessageBlocking(
      RealTimeRoom const &room,
      MultiplayerParticipant const &participant,
      std::vector<uint8_t> data);

  /**
   * Sends a message to the specified <code>MultiplayerParticipant</code>s. Uses
   * an unreliable method to send the message. This method of sending data is
   * faster than sending data reliably and should be preferred if possible.
   */
  void SendUnreliableMessage(
      RealTimeRoom const &room,
      std::vector<MultiplayerParticipant> const &participants,
      std::vector<uint8_t> data);

  /**
   * Sends a message to all participants other than the current user. Uses an
   * unreliable method to send the message. This method of sending data is
   * faster than sending data reliably and should be preferred if possible.
   */
  void SendUnreliableMessageToOthers(RealTimeRoom const &room,
                                     std::vector<uint8_t> data);

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>ShowRoomInboxUI</code> operation. If <code>IsSuccess(status)</code>
   * returns true, then <code>room</code> is <code>Valid()</code>.
   *
   * @ingroup ResponseType
   */
  struct RoomInboxUIResponse {
    /**
     * The <code>UIStatus</code> of the operation that generated this
     * <code>Response</code>.
     */
    UIStatus status;

    /**
     * The <code>MultiplayerInvitation</code> for this response.
     * <code>Valid()</code> only returns true for the room if
     * <code>IsSuccess(status)</code> returns true.
     */
    MultiplayerInvitation invitation;
  };

  /**
   * Defines a callback that can receive a <code>RoomInboxUIResponse</code>
   * from <code>ShowRoomInboxUI</code>.
   */
  typedef std::function<void(RoomInboxUIResponse const &)> RoomInboxUICallback;

  /**
   * Asynchronously shows the room inbox UI, allowing the player to select a
   * multiplayer invitation. Upon completion, the selected invitation is
   * returned via the <code>RoomInboxUIResponse</code>.
   */
  void ShowRoomInboxUI(RoomInboxUICallback callback);

  /**
   * Blocking version of {@link ShowRoomInboxUI}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  RoomInboxUIResponse ShowRoomInboxUIBlocking(Timeout timeout);

  /**
   * Overload of {@link ShowRoomInboxUIBlocking}, which uses a default
   * timeout of 10 years.
   */
  RoomInboxUIResponse ShowRoomInboxUIBlocking();

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>ShowPlayerSelectUI</code> operation. This is shared with Turn Based
   * multiplayer.
   *
   * @ingroup ResponseType
   */
  typedef TurnBasedMultiplayerManager::PlayerSelectUIResponse
      PlayerSelectUIResponse;

  /**
   * Defines a callback that can receive a <code>PlayerSelectUIResponse</code>
   * from <code>ShowPlayerSelectUI</code>.
   */
  typedef std::function<void(PlayerSelectUIResponse const &)>
      PlayerSelectUICallback;

  /**
   * Asynchronously shows the player select UI, allowing the player to select
   * other players to join a room with. Upon completion, the selected players
   * will be returned via the <code>PlayerSelectUICallback</code>.
   */
  void ShowPlayerSelectUI(uint32_t minimum_players,
                          uint32_t maximum_players,
                          bool allow_automatch,
                          PlayerSelectUICallback callback);

  /**
   * Blocking version of {@link ShowPlayerSelectUI}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  PlayerSelectUIResponse ShowPlayerSelectUIBlocking(Timeout timeout,
                                                    uint32_t minimum_players,
                                                    uint32_t maximum_players,
                                                    bool allow_automatch);

  /**
   * Overload of {@link ShowPlayerSelectUIBlocking}, which uses a default
   * timeout of 10 years.
   */
  PlayerSelectUIResponse ShowPlayerSelectUIBlocking(uint32_t minimum_players,
                                                    uint32_t maximum_players,
                                                    bool allow_automatch);

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>ShowWaitingRoomUI</code> operation. If <code>IsSuccess(status)</code>
   * returns true, <code>room</code> is populated.
   *
   * @ingroup ResponseType
   */
  struct WaitingRoomUIResponse {
    /**
     * The <code>ResponseStatus</code> of the operation which generated this
     * <code>Response</code>.
     */
    UIStatus status;

    /**
     * The <code>RealTimeRoom</code> for this response. <code>Valid()</code>
     * only returns true for the room if <code>IsSuccess()</code> returns true
     * for <code>status</code>.
     */
    RealTimeRoom room;
  };

  /**
   * Defines a callback that can be used to receive a
   * <code>WaitingRoomUIResponse</code> from one of the real-time multiplayer
   * operations.
   */
  typedef std::function<void(WaitingRoomUIResponse const &)>
      WaitingRoomUICallback;

  /**
   * Shows a waiting room UI which displays the status of
   * MultiplayerParticipants joining this room.
   */
  void ShowWaitingRoomUI(RealTimeRoom const &room,
                         uint32_t min_participants_to_start,
                         WaitingRoomUICallback callback);

  /**
   * Blocking version of {@link ShowWaitingRoomUI}. Allows the caller to specify
   * a timeout in ms. After the sepcified time elapses, the function will return
   * <code>ERROR_TIMEOUT</code>.
   */
  WaitingRoomUIResponse ShowWaitingRoomUIBlocking(
      Timeout timeout,
      RealTimeRoom const &room,
      uint32_t min_participants_to_start);

  /**
   * Overload of {@link ShowWaitingRoomUIBlocking}, which uses a default
   * timeout of 10 years.
   */
  WaitingRoomUIResponse ShowWaitingRoomUIBlocking(
      RealTimeRoom const &room, uint32_t min_participants_to_start);

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>FetchInvitations</code> operation. If <code>IsSuccess(status)</code>
   * returns true, <code>invitations</code> vector populated.
   *
   * @ingroup ResponseType
   */
  struct FetchInvitationsResponse {
    /**
     * The <code>ResponseStatus</code> of the operation which generated this
     * <code>Response</code>.
     */
    ResponseStatus status;

    /**
     * The vector of <code>MultiplayerInvitation</code>s for this response.
     */
    std::vector<MultiplayerInvitation> invitations;
  };

  /**
   * Defines a callback that can be used to receive a
   * <code>WaitingRoomUIResponse</code> from one of the real-time multiplayer
   * operations.
   */
  typedef std::function<void(FetchInvitationsResponse const &)>
      FetchInvitationsCallback;

  /**
   * Fetches any <code>MultiplayerInvitation</code>s for real time rooms. The
   * fetched <code>MultiplayerInvitation</code>s are returned via the provided
   * <code>FetchInvitationsCallback</code>.
   */
  void FetchInvitations(FetchInvitationsCallback callback);

  /**
   * Blocking version of {@link FetchInvitations}. Allows the caller to specify
   * a timeout in ms. After the sepcified time elapses, the function will return
   * <code>ERROR_TIMEOUT</code>.
   */
  FetchInvitationsResponse FetchInvitationsBlocking(Timeout timeout);

  /**
   * Overload of {@link FetchInvitationsBlocking}, which uses a default
   * timeout of 10 years.
   */
  FetchInvitationsResponse FetchInvitationsBlocking();

 private:
  friend class GameServicesImpl;
  explicit RealTimeMultiplayerManager(GameServicesImpl *game_services_impl);
  ~RealTimeMultiplayerManager();
  RealTimeMultiplayerManager(RealTimeMultiplayerManager const &) = delete;
  RealTimeMultiplayerManager &operator=(RealTimeMultiplayerManager const &) =
      delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_REAL_TIME_MULTIPLAYER_MANAGER_H_
