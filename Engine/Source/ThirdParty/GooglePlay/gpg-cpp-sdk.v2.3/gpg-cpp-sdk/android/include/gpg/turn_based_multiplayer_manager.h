// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/turn_based_multiplayer_manager.h
 *
 * @brief Entry points for Play Games Turn Based Multiplayer functionality.
 */

#ifndef GPG_TURN_BASED_MULTIPLAYER_MANAGER_H_
#define GPG_TURN_BASED_MULTIPLAYER_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <vector>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/multiplayer_invitation.h"
#include "gpg/player.h"
#include "gpg/turn_based_match.h"
#include "gpg/turn_based_match_config.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Fetches, modifies and creates <code>TurnBasedMatch</code> objects.
 * @ingroup Managers
 */
class GPG_EXPORT TurnBasedMultiplayerManager {
 public:
  /**
   * A participant which can be passed to methods which take a "next
   * participant". This causes the method to select the next participant via
   * automatching. It is only valid to pass kAutomatchingParticipant to a
   * function if {@link TurnBasedMatch::AutomatchingSlotsAvailable} is more than
   * 0 for the related match.
   */
  static const MultiplayerParticipant kAutomatchingParticipant;

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for a specific
   * <code>TurnBasedMatch</code>. The match value is only valid if
   * <code>IsSuccess()</code> returns true for <code>ResponseStatus</code>.
   *
   * @ingroup ResponseType
   */
  struct TurnBasedMatchResponse {
    /**
     * The <code>ResponseStatus</code> of the operation that generated this
     * response.
     */
    MultiplayerStatus status;

    /**
     * The <code>TurnBasedMatch</code> for this response. <code>Valid()</code>
     * only returns true for the match if <code>IsSuccess()</code> returns true
     * for <code>status</code>.
     */
    TurnBasedMatch match;
  };

  /**
   * Defines a callback that can be used to receive a
   * <code>TurnBasedMatchResponse</code> from one of the turn-based multiplayer
   * operations.
   */
  typedef std::function<void(TurnBasedMatchResponse const &)>
      TurnBasedMatchCallback;

  /**
   * Asynchronously creates a <code>TurnBasedMatch</code> using the provided
   * <code>TurnBasedMatchConfig</code>. If creation is successful, this
   * function returns the <code>TurnBasedMatch</code> via the provided
   * <code>TurnBasedMatchCallback</code>. A newly created
   * <code>TurnBasedMatch</code> always starts in the
   * <code>TurnBasedMatchState::MY_TURN</code> state.
   */
  void CreateTurnBasedMatch(gpg::TurnBasedMatchConfig const &config,
                            TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link CreateTurnBasedMatch}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse CreateTurnBasedMatchBlocking(
      Timeout timeout, gpg::TurnBasedMatchConfig const &config);

  /**
   * Overload of {@link CreateTurnBasedMatchBlocking}, which uses a default
   * timeout of 10 years.
   */
  TurnBasedMatchResponse CreateTurnBasedMatchBlocking(
      gpg::TurnBasedMatchConfig const &config);

  /**
   * Asynchronously accepts a <code>MultiplayerInvitation</code>, and returns
   * the result via a <code>TurnBasedMatchCallback</code>. If the operation is
   * successful, the <code>TurnBasedMatch</code> returned via the callback is in
   * the <code>TurnBasedMatchState::MY_TURN</code> state.
   */
  void AcceptInvitation(MultiplayerInvitation const &invitation,
                        TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link AcceptInvitation}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse AcceptInvitationBlocking(
      Timeout timeout, MultiplayerInvitation const &invitation);

  /**
   * Overload of {@link AcceptInvitationBlocking}, which uses a default timeout
   * of 10 years.
   */
  TurnBasedMatchResponse AcceptInvitationBlocking(
      MultiplayerInvitation const &invitation);

  /**
   * Declines a <code>MultiplayerInvitation</code> to a
   * <code>TurnBasedMatch</code>. Doing so cancels the match for the other
   * participants, and removes the match from the local player's device.
   */
  void DeclineInvitation(MultiplayerInvitation const &invitation);

  /**
   * Dismisses a <code>MultiplayerInvitation</code> to a
   * <code>TurnBasedMatch</code>. This does not change the visible state of the
   * <code>TurnBasedMatch</code> for the other participants, but removes the
   * <code>TurnBasedMatch</code> from the local player's device.
   */
  void DismissInvitation(MultiplayerInvitation const &invitation);

  /**
   * Dismisses a <code>TurnBasedMatch</code>. This does not change the visible
   * state of the <code>TurnBasedMatch</code> for the other participants, but
   * removes the <code>TurnBasedMatch</code> from the local player's device.
   */
  void DismissMatch(TurnBasedMatch const &match);

  /**
   * Asynchronously fetches a specific match by id. The result of this operation
   * is returned via a <code>TurnBasedMatchCallback</code>.
   */
  void FetchMatch(std::string const &match_id, TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link FetchMatch}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse FetchMatchBlocking(Timeout timeout,
                                            std::string const &match_id);

  /**
   * Overload of {@link FetchMatchBlocking}, which uses a default timeout of 10
   * years.
   */
  TurnBasedMatchResponse FetchMatchBlocking(std::string const &match_id);

  /**
   * Asynchronously takes the local participant's turn. When taking a turn, the
   * participant may specify a new value for <code>match_data</code>, as well as
   * a set of <code>ParticipantResults</code>. When the turn is over, the
   * updated match is returned via the <code>TurnBasedMatchCallback</code>. This
   * function may only be called when <code>TurnBasedMatch::Status()</code> is
   * <code>MatchStatus::MY_TURN</code>.
   *
   * @param match The match where the turn takes place.
   * @param match_data A blob of data to send to the next participant.
   * @param results Any known results for the match at the current time. Note
   *     that the result for a given player may only be specified once.
   *     Attempting to set different results for a player results in
   *     <code>ERROR_INVALID_RESULTS</code>.
   * @param next_participant The participant whose turn is next.
   *     {@link TurnBasedMultiplayerManager::kAutomatchingParticipant} may be
   *     used to specify that the next participant should be selected via
   *     auto-matching.
   * @param callback The callback that receives the
   *     <code>TurnBasedMatchResponse</code>.
   */
  void TakeMyTurn(TurnBasedMatch const &match,
                  std::vector<uint8_t> match_data,
                  ParticipantResults const &results,
                  MultiplayerParticipant const &next_participant,
                  TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link TakeMyTurn}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse TakeMyTurnBlocking(
      Timeout timeout,
      TurnBasedMatch const &match,
      std::vector<uint8_t> match_data,
      ParticipantResults const &results,
      MultiplayerParticipant const &next_participant);

  /**
   * Overload of {@link TakeMyTurnBlocking}, which uses a default timeout of 10
   * years.
   */
  TurnBasedMatchResponse TakeMyTurnBlocking(
      TurnBasedMatch const &match,
      std::vector<uint8_t> match_data,
      ParticipantResults const &results,
      MultiplayerParticipant const &next_participant);

  /**
   * Asynchronously finishes the specified match. This can be used rather than
   * {@link TakeMyTurn} during the final turn of the match. Allows the caller to
   * specify a final value for <code>match_data</code>, as well as a set of
   * final values for <code>ParticipantResults</code>. After this operation is
   * completed, the updated match is returned via the provided
   * <code>TurnBasedMatchCallback.</code> This function can only be called when
   * <code>TurnBasedMatch::Status()</code> returns
   * <code>MatchStatus::MY_TURN.</code>
   *
   * @param match The match to finish.
   * @param match_data A blob of data representing the final state of the match.
   * @param results Any results for each player in the match. Note that these
   *     results must not contradict any results specified earlier via
   *     <code>TakeTurn</code>. Attempting to set different results for a player
   *     results in <code>ERROR_INVALID_RESULTS</code>.
   * @param callback The callback that receives the
   *     <code>TurnBasedMatchResponse</code>.
   */
  void FinishMatchDuringMyTurn(TurnBasedMatch const &match,
                               std::vector<uint8_t> match_data,
                               ParticipantResults const &results,
                               TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link FinishMatchDuringMyTurn}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse FinishMatchDuringMyTurnBlocking(
      Timeout timeout,
      TurnBasedMatch const &match,
      std::vector<uint8_t> match_data,
      ParticipantResults const &results);

  /**
   * Overload of {@link FinishMatchDuringMyTurnBlocking}, which uses a default
   * timeout of 10 years.
   */
  TurnBasedMatchResponse FinishMatchDuringMyTurnBlocking(
      TurnBasedMatch const &match,
      std::vector<uint8_t> match_data,
      ParticipantResults const &results);

  /**
   * Confirms the results of a match that has ended and is pending
   * local completion. This function can only be called when
   * <code>TurnBasedMatch::Status()</code> returns
   * <code>MatchStatus::PENDING_COMPLETION</code>.
   *
   * @param match The match whose completion to confirm.
   * @param callback The callback receiving a
   *     <code>TurnBasedMatchResponse</code>.
   */
  void ConfirmPendingCompletion(TurnBasedMatch const &match,
                                TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link ConfirmPendingCompletion}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse ConfirmPendingCompletionBlocking(
      Timeout timeout, TurnBasedMatch const &match);

  /**
   * Overload of {@link ConfirmPendingCompletionBlocking}, which uses a default
   * timeout of 10 years.
   */
  TurnBasedMatchResponse ConfirmPendingCompletionBlocking(
      TurnBasedMatch const &match);

  /**
   * Rematches a match whose state is MatchStatus::COMPLETED. If the rematch
   * is possible, <code>TurnBasedMatchCallback</code> receives the new match.
   *
   * @param match The match to rematch.
   * @param callback The callback that receives a TurnBasedMatchResponse.
   */
  void Rematch(TurnBasedMatch const &match, TurnBasedMatchCallback callback);

  /**
   * Blocking version of {@link Rematch}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchResponse RematchBlocking(Timeout timeout,
                                         TurnBasedMatch const &match);

  /**
   * Overload of {@link RematchBlocking}, which uses a default timeout of 10
   * years.
   */
  TurnBasedMatchResponse RematchBlocking(TurnBasedMatch const &match);

  /**
   * Defines a callback which can be used to receive a MultiplayerStatus. Used
   * by the LeaveMatch* and CancelMatch* functions.
   */
  typedef std::function<void(MultiplayerStatus)> MultiplayerStatusCallback;

  /**
   * Asynchronously leaves a match during another participant's turn. The
   * response returned via the <code>MultiplayerStatusCallback</code> contains
   * whether the local participant left the match successfully.
   * If this departure leaves the match with fewer than two participants, the
   * match is canceled. <code>match.Status()</code> must return
   * <code>MatchStatus::THEIR_TURN</code> for this function to be usable.
   */
  void LeaveMatchDuringTheirTurn(TurnBasedMatch const &match,
                                 MultiplayerStatusCallback callback);

  /**
   * Blocking version of {@link LeaveMatchDuringTheirTurn}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  MultiplayerStatus LeaveMatchDuringTheirTurnBlocking(
      Timeout timeout, TurnBasedMatch const &match);

  /**
   * Overload of {@link LeaveMatchDuringTheirTurnBlocking}, which uses a default
   * timeout of 10 years.
   */
  MultiplayerStatus LeaveMatchDuringTheirTurnBlocking(
      TurnBasedMatch const &match);

  /**
   * Asynchronously leaves a match during the local participant's turn. The
   * response returned via the <code>TurnBasedMatchCallback</code> contains the
   * state of the match after the local player has gone. If this departure
   * leaves the match with fewer than two participants, the match is canceled.
   * <code>match.Status()</code> must return <code>MatchStatus::MY_TURN</code>
   * for this function to be usable.
   *
   * @param match The match to leave.
   * @param next_participant The participant whose turn is next.
   *     {@link TurnBasedMultiplayerManager::kAutomatchingParticipant} may be
   *     used to specify that the next participant should be selected via
   *     auto-matching.
   * @param callback The callback that receives the
   *     <code>TurnBasedMatchResponse</code>
   */
  void LeaveMatchDuringMyTurn(TurnBasedMatch const &match,
                              MultiplayerParticipant const &next_participant,
                              MultiplayerStatusCallback callback);

  /**
   * Blocking version of {@link LeaveMatchDuringMyTurn}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  MultiplayerStatus LeaveMatchDuringMyTurnBlocking(
      Timeout timeout,
      TurnBasedMatch const &match,
      MultiplayerParticipant const &next_participant);

  /**
   * Overload of {@link LeaveMatchDuringMyTurnBlocking}, which uses a default
   * timeout of 10 years.
   */
  MultiplayerStatus LeaveMatchDuringMyTurnBlocking(
      TurnBasedMatch const &match,
      MultiplayerParticipant const &next_participant);

  /**
   * Asynchronously cancels a match. The status returned via the
   * <code>MultiplayerStatusCallback</code> indicates whether the operation
   * succeeded. Match status must INVITED, THEIR_TURN, or MY_TURN for this
   * function to be usable.
   */
  void CancelMatch(TurnBasedMatch const &match,
                   MultiplayerStatusCallback callback);

  /**
   * Blocking version of {@link CancelMatch}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  MultiplayerStatus CancelMatchBlocking(Timeout timeout,
                                        TurnBasedMatch const &match);

  /**
   * Overload of {@link CancelMatch} which uses a default timeout of 10 years.
   */
  MultiplayerStatus CancelMatchBlocking(TurnBasedMatch const &match);

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for
   * {@link TurnBasedMatch TurnBasedMatches} and
   * {@link MultiplayerInvitation invitations}. If <code>IsSuccess()</code> does
   * not return true for <code>ResponseStatus</code>, then <code>empty()</code>
   * returns true for a vector of invitations.
   *
   * @ingroup ResponseType
   */
  struct TurnBasedMatchesResponse {
    /**
     * The <code>MultiplayerStatus</code> of the operation that generated this
     * <code>Response</code>.
     */
    MultiplayerStatus status;

    /**
     * The data field includes all {@link MultiplayerInvitation}s and
     * {@link TurnBasedMatch}es, grouped for conventient display in UI.
     * This struct will only contain valid data if
     * <code>IsSuccess(status)</code>.
     */
    struct {
      /**
       * A vector of all {@link MultiplayerInvitation}s. Invitations are sorted
       * by last update time.
       */
      std::vector<MultiplayerInvitation> invitations;

      /**
       * A vector of {@link TurnBasedMatch}es with {@link MatchStatus}
       * <code>MY_TURN</code> or <code>PENDING_COMPLETION</code>. Matches are
       * sorted by last update time.
       */
      std::vector<TurnBasedMatch> my_turn_matches;

      /**
       * A vector of {@link TurnBasedMatch}es with {@link MatchStatus}
       * <code>THEIR_TURN</code>. Matches are sorted by last update time.
       */
      std::vector<TurnBasedMatch> their_turn_matches;

      /**
       * A vector of {@link TurnBasedMatch}es with {@link MatchStatus}
       * <code>COMPLETED</code>. Matches are sorted by last update time.
       */
      std::vector<TurnBasedMatch> completed_matches;
    } data;
  };

  /**
   * Defines a callback that can receive a <code>TurnBasedMatchesResponse</code>
   * from one of the turn-based multiplayer operations.
   */
  typedef std::function<void(TurnBasedMatchesResponse const &)>
      TurnBasedMatchesCallback;

  /**
   * Asynchronously fetches <code>TurnBasedMatch</code> and
   * <code>Invitation</code> objects for the current player. All active matches
   * and up to 10 completed matches are returned.
   */
  void FetchMatches(TurnBasedMatchesCallback callback);

  /**
   * Blocking version of {@link FetchMatches}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  TurnBasedMatchesResponse FetchMatchesBlocking(Timeout timeout);

  /**
   * Overload of {@link FetchMatchesBlocking}, which uses a default timeout of
   * 10 years.
   */
  TurnBasedMatchesResponse FetchMatchesBlocking();

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>ShowMatchInboxUI</code> operation. If <code>IsSuccess(status)</code>
   * returns true, <code>Valid()</code> returns true for exactly one invitation
   * or match. Otherwise, it does not return true for either of them.
   *
   * @ingroup ResponseType
   */
  struct MatchInboxUIResponse {
    /**
     * The <code>UIStatus</code> of the operation that generated this
     * <code>Response</code>.
     */
    UIStatus status;

    /**
     * The <code>TurnBasedMatch</code> for this response. <code>Valid()</code>
     * only returns true for the match if <code>IsSuccess(status)</code> returns
     * true.
     */
    TurnBasedMatch match;
  };

  /**
   * Defines a callback that can receive a <code>MatchInboxUIResponse</code>
   * from <code>ShowMatchInboxUI</code>.
   */
  typedef std::function<void(MatchInboxUIResponse const &)>
      MatchInboxUICallback;

  /**
   * Asynchronously shows the match inbox UI, allowing the player to select a
   * match or invitation. Upon completion, the selected match or invitation is
   * returned via the <code>MatchInboxUICallback</code>.
   */
  void ShowMatchInboxUI(MatchInboxUICallback callback);

  /**
   * Blocking version of {@link ShowMatchInboxUI}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  MatchInboxUIResponse ShowMatchInboxUIBlocking(Timeout timeout);

  /**
   * Overload of {@link ShowMatchInboxUIBlocking}, which uses a default
   * timeout of 10 years.
   */
  MatchInboxUIResponse ShowMatchInboxUIBlocking();

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>ShowPlayerSelectUI</code> operation. If
   * <code>IsSuccess(status)</code> returns true, the remaining fields are
   * populated.
   *
   * @ingroup ResponseType
   */
  struct PlayerSelectUIResponse {
    /**
     * The <code>ResponseStatus</code> of the operation which generated this
     * <code>Response</code>.
     */
    UIStatus status;

    /**
     * A list of players whom the player has selected to invite to a match.
     */
    std::vector<std::string> player_ids;

    /**
     * The minimum number of auto-matching players to use.
     */
    uint32_t minimum_automatching_players;

    /**
     * The maximum number of auto-matching players to use.
     */
    uint32_t maximum_automatching_players;
  };

  /**
   * Defines a callback that can receive a <code>PlayerSelectUIResponse</code>
   * from <code>ShowPlayerSelectUI</code>.
   */
  typedef std::function<void(PlayerSelectUIResponse const &)>
      PlayerSelectUICallback;

  /**
   * Asynchronously shows the player select UI, allowing the player to select
   * other players to play a match with. Upon completion, the selected players
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
   * Forces a sync of TBMP match data with the server. Receipt of new data
   * triggers an <code>OnTurnBasedMatchEventCallback</code> or an
   * <code>OnMultiplayerInvitationReceivedCallback</code>.
   */
  void SynchronizeData();

 private:
  friend class GameServicesImpl;
  explicit TurnBasedMultiplayerManager(GameServicesImpl *game_services_impl);
  ~TurnBasedMultiplayerManager();
  TurnBasedMultiplayerManager(TurnBasedMultiplayerManager const &) = delete;
  TurnBasedMultiplayerManager &operator=(TurnBasedMultiplayerManager const &) =
      delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_TURN_BASED_MULTIPLAYER_MANAGER_H_
