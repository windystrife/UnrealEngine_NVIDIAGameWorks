// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/score_summary.h
 *
 * @brief Value object that represents a summary of the scores for a particular
 * variant of a Leaderboard.
 */

#ifndef GPG_SCORE_SUMMARY_H_
#define GPG_SCORE_SUMMARY_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <string>
#include "gpg/common.h"
#include "gpg/score_page.h"
#include "gpg/types.h"

namespace gpg {

class ScoreSummaryImpl;

/**
 * A single data structure which allows you to access a summary of score
 * information.
 * @ingroup ValueType
 */
class GPG_EXPORT ScoreSummary {
 public:
  ScoreSummary();

  /**
   * Constructs a <code>ScoreSummary</code> from a <code>shared_ptr</code> to
   * a <code>ScoreSummaryImpl</code>.
   * Intended for internal use by the API.
   */
  explicit ScoreSummary(std::shared_ptr<ScoreSummaryImpl const> impl);

  /**
   * Creates a copy of an existing <code>ScoreSummary</code>.
   */
  ScoreSummary(ScoreSummary const &copy_from);

  /**
   * Moves an existing <code>ScoreSummary</code>.
   */
  ScoreSummary(ScoreSummary &&move_from);

  /**
   * Assigns this <code>ScoreSummary</code> by copying from another one.
   */
  ScoreSummary &operator=(ScoreSummary const &copy_from);

  /**
   * Assigns this <code>ScoreSummary</code> by moving another one into it.
   */
  ScoreSummary &operator=(ScoreSummary &&move_from);

  ~ScoreSummary();

  /**
   * Returns true if this <code>ScoreSummary</code> is populated with data.
   * Must return true for the getter functions on the
   * <code>ScoreSummary</code> object (<code>LeaderboardId</code>,
   * <code>TimeSpan</code>, etc...) to be usable.
   */
  bool Valid() const;

  // Properties that define the leaderboard variant this score summary is based
  // on.

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to a leaderboard in your game client.
   * It can only be called when Leaderboard::Valid() returns true.
   */
  std::string const &LeaderboardId() const;

  /**
   * Returns the leaderboard timespan.
   * Possible values are DAILY, WEEKLY, or ALL_TIME.
   */
  LeaderboardTimeSpan TimeSpan() const;

  /**
   * Returns the collection to which the leaderboard belongs.
   * Possible values are PUBLIC and SOCIAL.
   */
  LeaderboardCollection Collection() const;

  // Data for this summary

  /**
   * Returns the approximate number of scores on the score page.
   * Returns an error if no scores have been requested (max_results = 0), and
   * clamps the number at 25 if there are an excessive number of them.
   */
  uint64_t ApproximateNumberOfScores() const;

  /// Returns the score for the currently signed-in player.
  Score const &CurrentPlayerScore() const;

  /**
   * Returns whether there is a rank for the currently signed-in player.
   */

 private:
  std::shared_ptr<ScoreSummaryImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_SCORE_SUMMARY_H_
