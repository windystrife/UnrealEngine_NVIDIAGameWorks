// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/quest_manager.h
 *
 * @brief Entry points for Play Games Quest functionality.
 */

#ifndef GPG_QUEST_MANAGER_H_
#define GPG_QUEST_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/quest.h"
#include "gpg/quest_milestone.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Gets and sets various quest-related data.
 * @ingroup Managers
 */
class GPG_EXPORT QuestManager {
 public:
  /**
   * Holds data for a quest, along with a response status.
   * @ingroup ResponseType
   */
  struct FetchResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;

    /**
     * The fetched quest.
     */
    Quest data;
  };

  /**
   * Defines a callback type that receives a <code>FetchResponse</code>. This
   * callback type is provided to the <code>Fetch*</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchResponse const &)> FetchCallback;

  /**
   * Asynchronously loads data for a specific quest for the currently
   * signed-in player.
   * Calls the provided <code>FetchCallback</code> upon operation completion.
   * Not specifying <code>data_source</code> makes this function call
   * equivalent to calling
   * <code>Fetch(DataSource data_source, std::string const &quest_id, </code>
   * <code>FetchCallback callback)</code>, with <code>data_source</code>
   * specified as <code>CACHE_OR_NETWORK</code>.
   */
  void Fetch(std::string const &quest_id, FetchCallback callback);

  /**
   * Asynchronously loads quest data for the currently signed-in player.
   * Calls the provided <code>FetchCallback</code> upon operation completion.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>.
   */
  void Fetch(DataSource data_source,
             std::string const &quest_id,
             FetchCallback callback);

  /**
   * Synchronously loads quest data for the currently signed-in player,
   * directly returning the <code>FetchResponse</code>. Specifying neither
   * <code>data_source</code> nor <code>timeout</code> makes this function
   * call equivalent to calling
   * <code>FetchResponse FetchBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, std::string const &quest_id)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>
   * and <code>timeout</code> specified as 10 years.
   */
  FetchResponse FetchBlocking(std::string const &quest_id);

  /**
   * Synchronously loads all quest data for the currently signed-in player,
   * directly returning the <code>FetchResponse</code>. Specify
   * <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Not specifying timeout makes this
   * function call equivalent to calling
   * <code>FetchResponse FetchBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, std::string const &quest_id)</code>, with
   * <code>timeout</code> specified as 10 years.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              std::string const &quest_id);

  /**
   * Synchronously loads quest data for the currently signed-in player,
   * directly returning the <code>FetchResponse</code>. Specify
   * <code>timeout</code> as an arbitrary number of milliseconds. Not
   * specifying <code>data_source</code> makes this function call equivalent to
   * calling <code>FetchResponse FetchBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, std::string const &quest_id)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>.
   */
  FetchResponse FetchBlocking(Timeout timeout, std::string const &quest_id);

  /**
   * Synchronously loads quest data for the currently signed-in player,
   * directly returning the <code>FetchResponse</code>. Specify
   * <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify <code>timeout</code> as an arbitrary
   * number of milliseconds.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              Timeout timeout,
                              std::string const &quest_id);

  /**
   * Contains a list of quests and a response status.
   * @ingroup ResponseType
   */
  struct FetchListResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;

    /**
     * A vector containing quest data.
     */
    std::vector<Quest> data;
  };

  /**
   * Defines a callback type that receives a <code>FetchListResponse</code>.
   * This callback type is provided to the <code>FetchList*</code> functions
   * below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchListResponse const &)> FetchListCallback;

  /**
   * Asynchronously loads data for all quests matching the
   * <code>fetch_flags</code> bit-flags, which is made by ORing together
   * <code>QuestFetchFlags</code>.
   * Not specifying <code>data_source</code> makes this function call
   * equivalent to calling
   * <code>FetchList(DataSource data_source, int32_t fetch_flags,</code>
   * <code>FetchListCallback callback)</code>, with <code>data_source</code>
   * specified as <code>CACHE_OR_NETWORK</code>.
   */
  void FetchList(int32_t fetch_flags, FetchListCallback callback);

  /**
   * DEPRECATED since v1.2
   * Use this version:
   * void FetchList(int32_t fetch_flags, FetchListCallback callback);
   */
  void FetchList(FetchListCallback callback,
                 int32_t fetch_flags) GPG_DEPRECATED;

  /**
   * Asynchronously loads data for all quests matching the
   * <code>fetch_flags</code> bit-flags, which is made by ORing together
   * <code>QuestFetchFlags</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>.
   */
  void FetchList(DataSource data_source,
                 int32_t fetch_flags,
                 FetchListCallback callback);

  /**
   * Synchronously loads data for all quests matching the
   * <code>fetch_flags</code> bit-flags, which is made by ORing together
   * <code>QuestFetchFlags</code>.
   * It directly returns the <code>FetchListResponse</code>. Specifying neither
   * <code>data_source</code> nor <code>timeout</code> makes this function
   * call equivalent to calling
   * <code>FetchListResponse FetchListBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, int32_t fetch_flags)</code>, with
   * <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>, and
   * <code>timeout</code> specified as 10 years.
   */
  FetchListResponse FetchListBlocking(int32_t fetch_flags);

  /**
   * Synchronously loads data for all quests matching the
   * <code>fetch_flags</code> bit-flags, which is made by ORing together
   * <code>QuestFetchFlags</code>.
   * It directly returns the <code>FetchListResponse</code>.
   * Specify <code>data_source</code> as
   * <code>CACHE_OR_NETWORK</code> or <code>NETWORK_ONLY</code>. Not
   * specifying <code>timeout</code> makes this function call equivalent to
   * calling
   * <code>FetchListResponse FetchListBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, int32_t fetch_flags)</code>,
   * with <code>timeout</code> specified as 10 years.
   */
  FetchListResponse FetchListBlocking(DataSource data_source,
                                      int32_t fetch_flags);

  /**
   * Synchronously loads data for all quests matching the
   * <code>fetch_flags</code> bit-flags, which is made by ORing together
   * <code>QuestFetchFlags</code>.
   * It directly returns the <code>FetchListResponse</code>.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds. Not
   * specifying <code>data_source</code> makes
   * this function call equivalent to calling
   * <code>FetchListResponse FetchListBlocking(DataSource data_source, </code>
   * <code>Timeout timeout, int32_t fetch_flags)</code>, with
   * <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>, and
   * <code>timeout</code> containing the value you specified.
   */
  FetchListResponse FetchListBlocking(Timeout timeout, int32_t fetch_flags);

  /**
   * Synchronously loads data for all quests matching the
   * <code>fetch_flags</code> bit-flags, which is made by ORing together
   * <code>QuestFetchFlags</code>.
   * It directly returns the <code>FetchListResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify <code>timeout</code> as an arbitrary
   * number of milliseconds.
   */
  FetchListResponse FetchListBlocking(DataSource data_source,
                                      Timeout timeout,
                                      int32_t fetch_flags);

  /**
   * Asynchronously loads data for all quests, regardless of state, for the
   * currently signed-in player.
   * Not specifying <code>data_source</code> makes this function call
   * equivalent to calling
   * <code>FetchList(DataSource data_source, FetchListCallback callback)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>.
   */
  void FetchList(FetchListCallback callback);

  /**
   * Asynchronously loads data for all quests, regardless of state, for the
   * currently signed-in player.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code>
   * or <code>NETWORK_ONLY</code>.
   */
  void FetchList(DataSource data_source, FetchListCallback callback);

  /**
   * Synchronously loads data for all quests, regardless of state, for the
   * currently signed-in player, directly returning the
   * <code>FetchListResponse</code>.
   * Specifying neither <code>data_source</code> nor <code>timeout</code>
   * makes this function call equivalent to calling
   * <code>FetchListResponse FetchListBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>, with <code>data_source</code> specified as
   * <code>CACHE_OR_NETWORK</code>, and <code>timeout</code> specified as 10
   * years.
   */
  FetchListResponse FetchListBlocking();

  /**
   * Synchronously loads data for all quests, regardless of state, for the
   * currently signed-in player, directly returning the
   * <code>FetchListResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Not specifying <code>timeout</code> makes
   * this function call equivalent to calling
   * <code>FetchListResponse FetchListBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with <code>timeout</code> specified as 10 years.
   */
  FetchListResponse FetchListBlocking(DataSource data_source);

  /**
   * Synchronously loads data for all quests, regardless of state, for the
   * currently signed-in player, directly returning the
   * <code>FetchListResponse</code>.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds. Not
   * specifying <code>data_source</code> makes this function call equivalent
   * to calling
   * <code>FetchListResponse FetchListBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>.
   */
  FetchListResponse FetchListBlocking(Timeout timeout);

  /**
   * Synchronously loads data for all quests, regardless of state, for the
   * currently signed-in player, directly returning the
   * <code>FetchListResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify <code>timeout</code> as
   * an arbitrary number of milliseconds.
   */
  FetchListResponse FetchListBlocking(DataSource data_source, Timeout timeout);

  /**
   * Contains a response status and a quest.
   * <code>accepted_quest.Valid()</code> will return true if this quest was
   * successfully accepted.
   * @ingroup ResponseType
   */
  struct AcceptResponse {
    QuestAcceptStatus status;
    Quest accepted_quest;
  };

  /**
   * Defines a callback that can be used to receive an
   * <code>AcceptResponse</code>. Used by the <code>Accept*</code>
   * functions.
   * @ingroup Callbacks
   */
  typedef std::function<void(AcceptResponse const &)> AcceptCallback;

  /**
   * Asynchronously accept a quest. The quest must have a state
   * <code>QuestState::OPEN</code>. Incrementing the associated events will
   * start tracking progress toward the milestone goal.
   */
  void Accept(Quest const &quest, AcceptCallback callback);

  /**
   * Synchronously accept a quest. The quest must have a state
   * <code>QuestState::OPEN</code>. Incrementing the associated events will
   * start tracking progress toward the milestone goal.
   * Not specifying timeout makes this function call equivalent to calling
   * <code>AcceptResponse AcceptBlocking(Timeout timeout, </code>
   * <code>std::string const &quest_id)</code>, with <code>timeout</code>
   * specified as 10 years.
   */
  AcceptResponse AcceptBlocking(Quest const &quest);

  /**
   * Synchronously accept a quest. The quest must have a state
   * <code>QuestState::OPEN</code>. Incrementing the associated events will
   * start tracking progress toward the milestone goal.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   */
  AcceptResponse AcceptBlocking(Timeout timeout, Quest const &quest);

  /**
   * Contains a response status, a quest, and a milestone.
   * <code>claimed_milestone.Valid()</code> and <code>quest.Valid()</code> will
   * return true if the milestone was successfully claimed.
   * @ingroup ResponseType
   */
  struct ClaimMilestoneResponse {
    QuestClaimMilestoneStatus status;
    QuestMilestone claimed_milestone;
    Quest quest;
  };

  /**
   * Defines a callback which can be used to receive a
   * <code>ClaimMilestoneResponse</code>. Used by the
   * <code>ClaimMilestone*</code> functions.
   * @ingroup Callbacks
   */
  typedef std::function<void(ClaimMilestoneResponse const &)>
      ClaimMilestoneCallback;

  /**
   * Asynchronously claims the milestone. Doing so calls the server, marking
   * the milestone as completed. If the milestone is currently unclaimable, or
   * if it has been claimed already on this or another device, you will get an
   * error. If this call returns <code>QuestClaimMilestoneStatus::VALID</code>,
   * you (as a developer) must still reward the player. Use the milestone
   * <code>CompletionRewardData</code> to do so.
   */
  void ClaimMilestone(QuestMilestone const &milestone,
                      ClaimMilestoneCallback callback);

  /**
   * Synchronously claim the milestone. Doing so calls the server, marking the
   * milestone as completed. If the milestone is currently unclaimable, or
   * if it has been claimed already on this or another device, you will get an
   * error. If the response contains
   * <code>QuestClaimMilestoneStatus::VALID</code>, you (as a developer) must
   * still reward the player. Use the milestone
   * <code>CompletionRewardData</code> to do so.
   * Not specifying <code>timeout</code> makes this function call equivalent
   * to calling <code>ClaimMilestoneResponse</code>
   * ClaimMilestoneBlocking(Timeout timeout, QuestMilestone const &milestone)
   * with <code>timeout</code> specified as 10 years.
   */
  ClaimMilestoneResponse ClaimMilestoneBlocking(
      QuestMilestone const &milestone);

  /**
   * Synchronously claim the milestone. Doing so will call the server, marking
   * the milestone as completed. If the milestone is currently unclaimable, or
   * if it has been claimed already on this or another device, you will get an
   * error. If the response contains
   * <code>QuestClaimMilestoneStatus::VALID</code>, you (as a developer) must
   * still reward the player. Use the milestone
   * <code>CompletionRewardData</code> to do so.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   */
  ClaimMilestoneResponse ClaimMilestoneBlocking(
      Timeout timeout, QuestMilestone const &milestone);

  /**
   * Data and <code>UIStatus</code> for the <code>ShowAllUI</code> and
   * <code>ShowUI</code> operations. If <code>IsSuccess(status)</code>
   * returns true, <code>Valid()</code> returns true for exactly one of
   * <code>accepted_quest</code> or <code>milestone_to_claim</code>.
   * Otherwise, it does not return true for either of them.
   * If <code>accepted_quest</code> is valid, the user just accepted this quest.
   * If <code>milestone_to_claim</code> is valid, the user chose to claim this
   * milestone. You, the developer, then need to claim this by calling
   * <code>ClaimMilestone</code>.
   * @ingroup ResponseType
   */
  struct QuestUIResponse {
    /**
     * The <code>UIStatus</code> of the operation that generated this
     * <code>QuestUIResponse</code>.
     */
    UIStatus status;

    /**
     * The <code>accepted_quest</code> for this response. <code>Valid()</code>
     * only returns true for the quest if the user accepted this quest from the
     * UI.
     */
    Quest accepted_quest;

    /**
     * The <code>milestone_to_claim</code> for this response.
     * <code>Valid()</code> only returns true for the milestone if the user
     * chose to claim this milestone from the UI. You are required to call
     * <code>ClaimMilestone</code> for this milestone.
     */
    QuestMilestone milestone_to_claim;
  };

  /**
   * Defines a callback type that receives a <code>QuestUIResponse</code>. This
   * callback type is provided to the <code>ShowAllUI*</code> and
   * <code>ShowUI*</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(QuestUIResponse const &)> QuestUICallback;

  /**
   * Presents to the user a UI that displays information about all quests.
   * It asynchronously calls <code>QuestUICallback</code>.
   */
  void ShowAllUI(QuestUICallback callback);

  /**
   * Presents to the user a UI that displays information about all quests.
   * It synchronously returns a <code>QuestUIResponse</code>.
   * Not specifying <code>timeout</code> makes this function call equivalent
   * to calling <code>ShowAllUIBlocking(Timeout timeout)</code>
   * with <code>timeout</code> specified as 10 years.
   */
  QuestUIResponse ShowAllUIBlocking();

  /**
   * Presents to the user a UI that displays information about all quests.
   * It synchronously returns a <code>QuestUIResponse</code>.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   */
  QuestUIResponse ShowAllUIBlocking(Timeout timeout);

  /**
   * Presents to the user a UI that displays information about a specific
   * quest.
   * It asynchronously calls <code>QuestUICallback</code>.
   */
  void ShowUI(Quest const &quest, QuestUICallback callback);

  /**
   * Presents to the user a UI that displays information about a specific
   * quest.
   * It synchronously returns a <code>QuestUIResponse</code>.
   * Not specifying <code>timeout</code> makes this function call equivalent
   * to calling <code>ShowUIBlocking(Timeout timeout, Quest const &quest)</code>
   * with <code>timeout</code> specified as 10 years.
   */
  QuestUIResponse ShowUIBlocking(Quest const &quest);

  /**
   * Presents to the user a UI that displays information about a specific
   * quest.
   * It synchronously returns a <code>QuestUIResponse</code>.
   * Specify <code>timeout</code> as an arbitrary number of milliseconds.
   */
  QuestUIResponse ShowUIBlocking(Timeout timeout, Quest const &quest);

 private:
  friend class GameServicesImpl;
  explicit QuestManager(GameServicesImpl *game_services_impl);
  ~QuestManager();
  QuestManager(QuestManager const &) = delete;
  QuestManager &operator=(QuestManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_QUEST_MANAGER_H_
