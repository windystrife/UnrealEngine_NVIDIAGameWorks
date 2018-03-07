// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/leaderboard_manager.h
 *
 * @brief Entry points for Play Games Leaderboard functionality.
 */

#ifndef GPG_LEADERBOARD_MANAGER_H_
#define GPG_LEADERBOARD_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/leaderboard.h"
#include "gpg/score_page.h"
#include "gpg/score_summary.h"
#include "gpg/types.h"

namespace gpg {

/**
 * Gets and sets various leaderboard-related data.
 * @ingroup Managers
 */
class GPG_EXPORT LeaderboardManager {
 public:
  /**
   * Gets a score page token for a specific leaderboard, starting by score or
   * player, and covering a specific time span and collection.
   * ScorePageToken is used in various Leaderboard functions that allow paging
   * through pages of scores.
   * Tokens created by this function will always start at the beginning of the
   * requested range.
   */
  ScorePage::ScorePageToken ScorePageToken(
      std::string const &leaderboard_id,
      LeaderboardStart start,
      LeaderboardTimeSpan time_span,
      LeaderboardCollection collection) const;

  /**
   * Holds data for a leaderboard, along with a response status.
   * @ingroup ResponseType
   */
  struct FetchResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;

    /**
     * Data associated with this app.
     */
    Leaderboard data;
  };

  /**
   * Defines a callback type that receives a FetchResponse. This callback type
   * is provided to the <code>Fetch(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchResponse const &)> FetchCallback;

  /**
   * Asynchronously loads leaderboard data for the currently signed-in player.
   * Calls the provided FetchCallback on operation completion.
   * Not specifying data_source makes this function call equivalent to
   * calling <code>Fetch(DataSource data_source, std::string const
   * &leaderboard_id, FetchCallback callback)</code>, with DataSource specified
   * as CACHE_OR_NETWORK.
   */
  void Fetch(std::string const &leaderboard_id, FetchCallback callback);

  /**
   * Asynchronously loads leaderboard data for the currently signed-in player.
   * Calls the provided FetchCallback on operation completion.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void Fetch(DataSource data_source,
             std::string const &leaderboard_id,
             FetchCallback callback);

  /**
   * Synchronously loads leaderboard data for the currently signed-in player,
   * directly returning the FetchResponse. Specifying neither data_source nor
   * timeout makes this function call equivalent to calling
   * FetchResponse FetchBlocking(DataSource data_source, Timeout timeout),
   * with data_source specified as CACHE_OR_NETWORK, and timeout specified as 10
   * years.
   */
  FetchResponse FetchBlocking(std::string const &leaderboard_id);

  /**
   * Synchronously loads leaderboard data for the currently signed-in player,
   * directly returning the FetchResponse. Specify data_source as
   * CACHE_OR_NETWORK or NETWORK_ONLY. Not specifying timeout makes this
   * function call equivalent to calling
   * FetchResponse FetchBlocking(DataSource data_source, Timeout timeout),
   * with your specified value for data_source, and timeout specified as 10
   * years.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              std::string const &leaderboard_id);

  /**
   * Synchronously loads leaderboard data for the currently signed-in player,
   * directly returning the FetchResponse. Specify timeout as an arbitrary
   * number of milliseconds. Not specifying data_source makes this function
   * call equivalent to calling
   * FetchResponse FetchBlocking(DataSource data_source, Timeout timeout),
   * with data_source specified as CACHE_OR_NETWORK, and timeout containing the
   * value you specified.
   */
  FetchResponse FetchBlocking(Timeout timeout,
                              std::string const &leaderboard_id);

  /**
   * Synchronously loads leaderboard data for the currently signed-in player.
   * directly returning the FetchResponse. Specify data_source as
   * CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout as an arbitrary number of
   * milliseconds.
   */
  FetchResponse FetchBlocking(DataSource data_source,
                              Timeout timeout,
                              std::string const &leaderboard_id);

  /**
   * Contains data and response statuses for all leaderboards.
   * Can be one of the values enumerated in {@link ResponseStatus}.
   * @ingroup ResponseType
   */
  struct FetchAllResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;

    /**
     * A vector containing all leaderboard data:
     */
    std::vector<Leaderboard> data;
  };

  /**
   * Defines a <code>FetchAllResponse</code>-type callback.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchAllResponse const &)> FetchAllCallback;

  /**
   * Asynchronously loads data for all leaderboards for the currently signed-in
   * player.
   * Not specifying data_source makes this function call equivalent to calling
   * FetchAll(DataSource data_source, FetchAllCallback callback), with
   * data_source specified as CACHE_OR_NETWORK.
   */
  void FetchAll(FetchAllCallback callback);

  /**
   * Asynchronously loads data for all leaderboards for the currently
   * signed-in player.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void FetchAll(DataSource data_source, FetchAllCallback callback);

  /**
   * Synchronously loads data for all leaderboards for the currently signed-in
   * player, directly returning the FetchAllResponse. Specifying neither
   * data_source nor timeout makes this function call
   * equivalent to calling FetchAllResponse FetchAllBlocking (DataSource
   * data_source, Timeout timeout), with data_source specified as
   * CACHE_OR_NETWORK, and timeout specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking();

  /**
   * Synchronously loads data for all leaderboards for the currently signed-in
   * player, directly returning the FetchAllResponse. Specify data_source
   * as CACHE_OR_NETWORK or NETWORK_ONLY. Not specifying timeout makes this
   * function call equivalent to calling
   * FetchAllResponse FetchAllBlocking(DataSource data_source, Timeout timeout),
   * with your specified data_source value, and timeout specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source);

  /**
   * Synchronously loads data for all leaderboards for the currently signed-in
   * player, directly returning the FetchAllResponse. Specify timeout as an
   * arbitrary number of milliseconds. Not specifying data_source makes this
   * function call equivalent to calling
   * FetchAllResponse FetchAllBlocking(DataSource data_source, Timeout timeout),
   * with data_source specified as CACHE_OR_NETWORK, and timeout containing the
   * value you specified.
   */
  FetchAllResponse FetchAllBlocking(Timeout timeout);

  /**
   * Synchronously loads data for all leaderboards for the currently signed-in
   * player, directly returning the FetchAllResponse. Specify data_source as
   * CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout as an arbitrary number of
   * milliseconds.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source, Timeout timeout);

  /**
   * Returns response status and data from the accessed score page.
   * @ingroup ResponseType
   */
  struct FetchScorePageResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;
    ScorePage data;
  };

  /**
   * Defines a <code>ScorePageResponse</code>-type callback.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchScorePageResponse const &)>
      FetchScorePageCallback;

  /**
   * Asynchronously returns data for a score page identified by score-page
   * token. Specifying neither data_source nor max_results makes this function
   * call equivalent to
   * FetchScorePage(DataSource data_source, ScorePage::ScorePageToken
   * const &token, uint32_t max_results, FetchScorePageCallback callback), with
   * data_source specified as CACHE_OR_NETWORK, and max_results specified as 20.
   */

  void FetchScorePage(ScorePage::ScorePageToken const &token,
                      FetchScorePageCallback callback);
  /**
   * Asynchronously returns data for a score page identified by score-page
   * token. Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   * Not specifying max_results makes this function call equivalent to
   * FetchScorePage(DataSource data_source, ScorePage::ScorePageToken
   * const &token, uint32_t max_results, FetchScorePageCallback callback), with
   * your specified value for data_source, and max_results specified as 20.
   */
  void FetchScorePage(DataSource data_source,
                      ScorePage::ScorePageToken const &token,
                      FetchScorePageCallback callback);
  /**
   * Asynchronously returns data for a score page identified by score-page
   * token. max_results specifies the maximum number of scores to include
   * on the resulting score page, which may be no larger than 25.
   * Not specifying data_source makes this function call equivalent to
   * FetchScorePage(DataSource data_source, ScorePage::ScorePageToken
   * const &token, uint32_t max_results, FetchScorePageCallback callback), with
   * data_source specified as CACHE_OR_NETWORK, and max_results containing your
   * specified value.
   */
  void FetchScorePage(ScorePage::ScorePageToken const &token,
                      uint32_t max_results,
                      FetchScorePageCallback callback);
  /**
   * Asynchronously returns data for a score page identified by score-page
   * token. Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY. max_results
   * specifies the maximum number of scores to include on the resulting score
   * page, which may be be no larger than 25.
   */
  void FetchScorePage(DataSource data_source,
                      ScorePage::ScorePageToken const &token,
                      uint32_t max_results,
                      FetchScorePageCallback callback);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specifying neither
   * data_source, timeout, nor max_results makes this function call equivalent
   * to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with data_source specified as CACHE_OR_NETWORK, timeout
   * specified as 10 years, and max_results specified as 20.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      ScorePage::ScorePageToken const &token);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specifying neither
   * timeout nor max_results makes this function call equivalent
   * to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with your specified data_source value, timeout
   * specified as 10 years, and max_results specified as 20.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      DataSource data_source, ScorePage::ScorePageToken const &token);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specify timeout in
   * milliseconds. Specifying neither data_source nor max_results makes this
   * function call equivalent to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with data_source specified as CACHE_OR_NETWORK, timeout
   * containing your specified value, and max_results specified as 20.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      Timeout timeout, ScorePage::ScorePageToken const &token);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specify a value up to 25 for
   * max_results. Specifying neither data_source nor timeout makes this
   * function call equivalent to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with data_source specified as CACHE_OR_NETWORK, timeout
   * specified as 10 years, and max_results containing your specified value.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      ScorePage::ScorePageToken const &token, uint32_t max_results);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specify data_source
   * as CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout in milliseconds.
   * Not specifying max_value makes this function call equivalent to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with data_source and timeout containing your specified
   * values, and max_results specified as 20.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      DataSource data_source,
      Timeout timeout,
      ScorePage::ScorePageToken const &token);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specify data_source
   * as CACHE_OR_NETWORK or NETWORK_ONLY, and max_results as a value up to 25.
   * Not specifying timeout makes this function call equivalent to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with data_source and max_results containing your specified
   * values, and timeout specified as 10 years.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      DataSource data_source,
      ScorePage::ScorePageToken const &token,
      uint32_t max_results);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse. Specify timeout in
   * milliseconds, and max_results as a value up to 25. Not specifying
   * data_source makes this function call equivalent to calling
   * FetchScorePageResponse FetchScorePageBlocking(DataSource data_source,
   * Timeout timeout, ScorePage::ScorePageToken const &token, uint32_t
   * max_results), with timeout and max_results containing your specified
   * values, and data_source specified as CACHE_OR_NETWORK.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      Timeout timeout,
      ScorePage::ScorePageToken const &token,
      uint32_t max_results);

  /**
   * Synchronously returns data for a score page identified by score-page token,
   * directly returning the FetchScorePageResponse.
   * Specify DataSource as CACHE_OR_NETWORK or NETWORK_ONLY.
   * Specify Timeout as an arbitrary number of milliseconds. Specify max_results
   * as a value up to 25.
   */
  FetchScorePageResponse FetchScorePageBlocking(
      DataSource data_source,
      Timeout timeout,
      ScorePage::ScorePageToken const &token,
      uint32_t max_results);

  /**
   * Data and response status for a specified leaderboard score summary.
   * A score summary is defined by the combination of the leaderboard's
   * collection (public or social) and time span (daily, weekly, or all-time).
   * @ingroup ResponseType
   */
  struct FetchScoreSummaryResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;

    /**
     * The specified score summary.
     */
    ScoreSummary data;
  };

  /**
   * Defines a <code>FetchScoreSummaryResponse</code>-type callback.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchScoreSummaryResponse const &)>
      FetchScoreSummaryCallback;

  /**
   * Asynchronously fetches all data for a specific leaderboard score summary,
   * which comprises a given leaderboard's collection and time span.
   * Specify DAILY, WEEKLY, or ALL_TIME for time span. Specify PUBLIC or SOCIAL
   * for collection.
   * Not specifying data_source makes this function call equivalent to
   * FetchScoreSummary(DataSource data_source, std::string const
   * &leaderboard_id, LeaderboardTimeSpan time_span, LeaderboardCollection
   * collection, FetchScoreSummaryCallback callback), with data_source specified
   * as CACHE_OR_NETWORK, and collection and time_span containing your specified
   * values.
   */
  void FetchScoreSummary(std::string const &leaderboard_id,
                         LeaderboardTimeSpan time_span,
                         LeaderboardCollection collection,
                         FetchScoreSummaryCallback callback);
  /**
   * Asynchronously fetches all data for a specific leaderboard score summary,
   * which comprises a given leaderboard's collection and time span.
   * Specify CACHE_OR_NETWORK or NETWORK_ONLY for data_source. Specify DAILY,
   * WEEKLY, or ALL_TIME for time_span. Specify PUBLIC or SOCIAL for collection.
   */
  void FetchScoreSummary(DataSource data_source,
                         std::string const &leaderboard_id,
                         LeaderboardTimeSpan time_span,
                         LeaderboardCollection collection,
                         FetchScoreSummaryCallback callback);

  /**
   * Synchronously fetches all data for a specific leaderboard score summary,
   * directly returning the FetchScoreSummaryResponse.
   * Specify DAILY, WEEKLY, or ALL_TIME for time span. Specify PUBLIC or SOCIAL
   * for collection.
   * Specifying neither data_source and timeout makes this function equivalent
   * to calling
   * FetchScoreSummaryResponse FetchScoreSummaryBlocking(DataSource data_source,
   * Timeout timeout, std::string const &leaderboard_id, LeaderboardTimeSpan
   * time_span, LeaderboardCollection collection), with data_source specified as
   * CACHE_OR_NETWORK, timeout specified as 10 years, and your specified values
   * for time_span and collection.
   */
  FetchScoreSummaryResponse FetchScoreSummaryBlocking(
      std::string const &leaderboard_id,
      LeaderboardTimeSpan time_span,
      LeaderboardCollection collection);

  /**
   * Synchronously fetches all data for a specific leaderboard score summary,
   * directly returning the FetchScoreSummaryResponse.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY. Specify DAILY,
   * WEEKLY, or ALL_TIME for time span. Specify PUBLIC or SOCIAL for collection.
   * Not specifying timeout makes this function equivalent to calling
   * FetchScoreSummaryResponse FetchScoreSummaryBlocking(DataSource data_source,
   * Timeout timeout, std::string const &leaderboard_id, LeaderboardTimeSpan
   * time_span, LeaderboardCollection collection), with your specified
   * data_source value, timeout specified as 10 years, and your specified values
   * for time_span and collection.
   */
  FetchScoreSummaryResponse FetchScoreSummaryBlocking(
      DataSource data_source,
      std::string const &leaderboard_id,
      LeaderboardTimeSpan time_span,
      LeaderboardCollection collection);

  /**
   * Synchronously fetches all data for a specific leaderboard score summary,
   * directly returning the FetchScoreSummaryResponse.
   * Specify timeout in milliseconds. Specify DAILY,
   * WEEKLY, or ALL_TIME for time span. Specify PUBLIC or SOCIAL for collection.
   * Not specifying data_source makes this function equivalent to calling
   * FetchScoreSummaryResponse FetchScoreSummaryBlocking(DataSource data_source,
   * Timeout timeout, std::string const &leaderboard_id, LeaderboardTimeSpan
   * time_span, LeaderboardCollection collection), with timeout specified as 10
   * years, and your specified values for data_source, time_span, and
   * collection.
   */
  FetchScoreSummaryResponse FetchScoreSummaryBlocking(
      Timeout timeout,
      std::string const &leaderboard_id,
      LeaderboardTimeSpan time_span,
      LeaderboardCollection collection);

  /**
   * Synchronously fetches all data for a specific leaderboard score summary,
   * directly returning the FetchScoreSummaryResponse.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout in
   * milliseconds. Specify DAILY, WEEKLY, or ALL_TIME for time span. Specify
   * PUBLIC or SOCIAL for collection.
   */
  FetchScoreSummaryResponse FetchScoreSummaryBlocking(
      DataSource data_source,
      Timeout timeout,
      std::string const &leaderboard_id,
      LeaderboardTimeSpan time_span,
      LeaderboardCollection collection);

  /**
   * Contains all data and response statuses for all leaderboard score
   * summaries.
   * A score summary is defined by the combination of the leaderboard's
   * collection (public or social) and time span (daily, weekly, or all-time).
   * @ingroup ResponseType
   */
  struct FetchAllScoreSummariesResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     */
    ResponseStatus status;
    /**
     * A vector of data for all score summaries.
     */
    std::vector<ScoreSummary> data;
  };

  /**
   * Defines a <code>FetchAllScoreSummariesResponse</code>-type callback.
   * @ingroup Callbacks
   */
  typedef std::function<void(FetchAllScoreSummariesResponse const &)>
      FetchAllScoreSummariesCallback;

  /**
   * Asynchronously fetches all score summaries for a specific leaderboard.
   * Not specifying data_source makes this function call equivalent to
   * FetchAllScoreSummaries(DataSource data_source,std::string const
   * &leaderboard_id, FetchAllScoreSummariesCallback callback), with data_source
   * specified as CACHE_OR_NETWORK.
   */
  void FetchAllScoreSummaries(std::string const &leaderboard_id,
                              FetchAllScoreSummariesCallback callback);
  /**
   * Asynchronously fetches all score summaries for a specific leaderboard.
   * Specify data_source as CACHE_OR_NETWORK or NETWORK_ONLY.
   */
  void FetchAllScoreSummaries(DataSource data_source,
                              std::string const &leaderboard_id,
                              FetchAllScoreSummariesCallback callback);

  /**
   * Synchronously fetches all score summaries for a specific leaderboard,
   * directly returning the FetchAllScoreSummariesResponse. Not specifying
   * data_source and timeout makes this function call equivalent to
   * FetchAllScoreSummariesResponse FetchAllScoreSummariesBlocking(
   * DataSource data_source, std::string const &leaderboard_id), with
   * data_source specified as CACHE_OR_NETWORK, and timeout specified as 10
   * years.
   */
  FetchAllScoreSummariesResponse FetchAllScoreSummariesBlocking(
      std::string const &leaderboard_id);
  /**
   * Synchronously fetches all score summaries for a specific leaderboard,
   * directly returning the FetchAllScoreSummariesResponse. Specify data_source
   * as CACHE_OR_NETWORK or NETWORK_ONLY. Not specifying timeout makes this
   * function call equivalent to FetchAllScoreSummariesResponse
   * FetchAllScoreSummariesBlocking(DataSource data_source, std::string const
   * &leaderboard_id), with your specified data_source value, and timeout
   * specified as 10 years.
   */
  FetchAllScoreSummariesResponse FetchAllScoreSummariesBlocking(
      DataSource data_source, std::string const &leaderboard_id);

  /**
   * Synchronously fetches all score summaries for a specific leaderboard,
   * directly returning the FetchAllScoreSummariesResponse. Specify timeout in
   * milliseconds. Not specifying data_source makes this function call
   * equivalent to FetchAllScoreSummariesResponse
   * FetchAllScoreSummariesBlocking(DataSource data_source, std::string const
   * &leaderboard_id), with data_source specified as CACHE_OR_NETWORK, and
   * timeout containing the value you specified for it.
   */
  FetchAllScoreSummariesResponse FetchAllScoreSummariesBlocking(
      Timeout timeout, std::string const &leaderboard_id);

  /**
   * Synchronously fetches all score summaries for a specific leaderboard,
   * directly returning the FetchAllScoreSummariesResponse. Specify data_source
   * as CACHE_OR_NETWORK or NETWORK_ONLY. Specify timeout in milliseconds.
   */
  FetchAllScoreSummariesResponse FetchAllScoreSummariesBlocking(
      DataSource data_source,
      Timeout timeout,
      std::string const &leaderboard_id);

  /**
   * Submit a score to the leaderboard for the currently signed-in player.
   * The score is ignored if it is worse (as defined by the leaderboard
   * configuration) than a previously submitted score for the same player.
   */
  void SubmitScore(std::string const &leaderboard_id, uint64_t score);

  /**
   * Submit, for the currently signed-in player, a score to the leaderboard
   * associated with a specific id and metadata (such as something the player
   * did to earn the score). The score is ignored if it is worse (as defined in
   * the leaderboard configuration) than a previously submitted score for the
   * same player.
   */
  void SubmitScore(std::string const &leaderboard_id,
                   uint64_t score,
                   std::string const &metadata);
  /**
   * Defines a callback type that receives a <code>UIStatus</code>.  This
   * callback type is provided to the <code>ShowAllUI*</code> function below.
   * @ingroup Callbacks
   */
  typedef std::function<void(UIStatus const &)> ShowAllUICallback;

  /**
   * Presents to the user a UI that displays information about all leaderboards.
   * It asynchronously calls <code>ShowAllUICallback</code>.
   */
  void ShowAllUI(ShowAllUICallback callback);

  /**
   * Presents to the user a UI that displays information about all leaderboards.
   * It synchronously returns a <code>UIStatus</code>.  Not specifying
   * <code>timeout</code> makes this function call equivalent to calling
   * <code>ShowAllUIBlocking(Timeout timeout)</code> with <code>timeout</code>
   * specified as 10 years.
   */
  UIStatus ShowAllUIBlocking();

  /**
   * Presents to the user a UI that displays information about all leaderboards.
   * It synchronously returns a <code>UIStatus</code>.  Specify
   * <code>timeout</code> as an arbitrary number of milliseconds.
   */
  UIStatus ShowAllUIBlocking(Timeout timeout);

  /**
   * @deprecated Prefer ShowAllUI(ShowAllUICallback callback).
   * Presents a UI to the user that displays information about all leaderboards.
   * The UI is shown asynchronously on all platforms.
   */
  void ShowAllUI() GPG_DEPRECATED;

  /**
   * Defines a callback type that receives a <code>UIStatus</code>.  This
   * callback type is provided to the <code>ShowUI*</code> function below.
   * @ingroup Callbacks
   */
  typedef std::function<void(UIStatus const &)> ShowUICallback;

  /**
   * Presents to the user a UI that displays information about a specific
   * leaderboard.  It asynchronously calls <code>ShowUICallback</code>.
   */
  void ShowUI(std::string const &leaderboard_id, ShowUICallback callback);

  /**
   * Presents to the user a UI that displays information about a specific
   * leaderboard.  It asynchronously calls <code>ShowUICallback</code>,
   * and will initially select the passed-in
   * <code>LeaderboardTimeSpan</code>.
   */
  void ShowUI(std::string const &leaderboard_id,
              LeaderboardTimeSpan time_span,
              ShowUICallback callback);

  /**
   * Presents to the user a UI that displays information about a specific
   * leaderboard. It synchronously returns a <code>UIStatus</code>.  Not
   * specifying <code>timeout</code> makes this function call equivalent to
   * calling
   * <code>ShowUIBlocking(string const &leaderboard_id, Timeout timeout)</code>
   * with <code>timeout</code> specified as 10 years.
   */
  UIStatus ShowUIBlocking(std::string const &leaderboard_id);

  /**
   * Presents to the user a UI that displays information about a specific
   * leaderboard. It synchronously returns a <code>UIStatus</code>.  Not
   * specifying <code>timeout</code> makes this function call equivalent to
   * calling
   * <code>ShowUIBlocking(string const &leaderboard_id, Timeout timeout)</code>
   * with <code>timeout</code> specified as 10 years.  The UI will initially
   * select the passed-in <code>LeaderboardTimeSpan</code>.
   */
  UIStatus ShowUIBlocking(std::string const &leaderboard_id,
                          LeaderboardTimeSpan time_span);

  /**
   * Presents to the user a UI that displays information about a specific
   * leaderboard.  It synchronously returns a <code>UIStatus</code>.  Specify
   * <code>timeout</code> as an arbitrary number of milliseconds.
   */
  UIStatus ShowUIBlocking(Timeout timeout, std::string const &leaderboard_id);

  /**
   * Presents to the user a UI that displays information about a specific
   * leaderboard.  It synchronously returns a <code>UIStatus</code>.  Specify
   * <code>timeout</code> as an arbitrary number of milliseconds.  The UI
   * will initially select the passed-in <code>LeaderboardTimeSpan</code>.
   */
  UIStatus ShowUIBlocking(Timeout timeout,
                          std::string const &leaderboard_id,
                          LeaderboardTimeSpan time_span);

  /**
   * @deprecated Prefer ShowUI(ShowUICallback callback).
   * Presents a UI to the user that displays information about a specific
   * leaderboard.  The UI is shown asynchronously on all platforms.
   */
  void ShowUI(std::string const &leaderboard_id) GPG_DEPRECATED;

 private:
  friend class GameServicesImpl;
  explicit LeaderboardManager(GameServicesImpl *game_services_impl);
  ~LeaderboardManager();
  LeaderboardManager(LeaderboardManager const &) = delete;
  LeaderboardManager &operator=(LeaderboardManager const &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_LEADERBOARD_MANAGER_H_
