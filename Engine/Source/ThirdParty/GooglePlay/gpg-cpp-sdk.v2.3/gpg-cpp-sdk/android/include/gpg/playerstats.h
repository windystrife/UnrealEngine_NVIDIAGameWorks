// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/playerstats.h
 *
 * @brief Value object that represents the collected statistics for a player.
 */

#ifndef GPG_PLAYERSTATS_H_
#define GPG_PLAYERSTATS_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include "gpg/common.h"
#include "gpg/types.h"

namespace gpg {

class PlayerStatsImpl;

/**
 * A data structure that allows you to access data about a specific player.
 * @ingroup ValueType
 */
class GPG_EXPORT PlayerStats {
 public:
  PlayerStats();

  /**
   * Constructs a PlayerStats from a <code>shared_ptr</code> to a
   * <code>PlayerStatsImpl</code>.
   * Intended for internal use by the API.
   */
  explicit PlayerStats(std::shared_ptr<PlayerStatsImpl const> impl);

  /**
   * Creates a copy of an existing PlayerStats object.
   */
  PlayerStats(PlayerStats const &copy_from);

  /**
   * Moves an existing PlayerStats object into a new one.
   */
  PlayerStats(PlayerStats &&move_from);

  /**
   * Assigns to this PlayerStats object by copying from another one.
   */
  PlayerStats &operator=(PlayerStats const &copy_from);

  /**
   * Assigns to this PlayerStats object by moving another one into it.
   */
  PlayerStats &operator=(PlayerStats &&move_from);
  ~PlayerStats();

  /**
   * Returns true when the returned PlayerStats object is populated with data
   * and is accompanied by a successful response status; false for an
   * unpopulated user-created PlayerStats or for a populated one accompanied by
   * an unsuccessful response status.
   * It must return true for the getter functions on this object to be usable.
   */
  bool Valid() const;

  /**
   * Returns whether or not this PlayerStats object has average session length
   * available. It must return true for <code>AverageSessionLength()</code>
   * to be usable.
   */
  bool HasAverageSessionLength() const;

  /**
   * Retrieves the average session length for this player, if known.
   * <code>HasAverageSessionLength()</code> must return true for this function
   * to be usable.
   */
  float AverageSessionLength() const;

  /**
   * Returns whether or not this PlayerStats object has churn probability
   * available. It must return true for <code>ChurnProbability()</code> to be
   * usable.
   */
  bool HasChurnProbability() const;

  /**
   * Retrieves the churn probability for this player, if known.
   * <code>HasChurnProbability()</code> must return true for this function
   * to be usable.
   */
  float ChurnProbability() const;

  /**
   * Returns whether or not this PlayerStats object has days since last played
   * available. It must return true for <code>DaysSinceLastPlayed()</code> to be
   * usable.
   */
  bool HasDaysSinceLastPlayed() const;

  /**
   * Retrieves the days since last played for this player, if known.
   * <code>HasDaysSinceLastPlayed()</code> must return true for this function to
   * be usable.
   */
  int32_t DaysSinceLastPlayed() const;

  /**
   * Returns whether or not this PlayerStats object has high spender probability
   * available. It must return true for <code>HighSpenderProbability()</code>
   * to be usable.
   */
  bool HasHighSpenderProbability() const;

  /**
   * Retrieves the high spender probability information for this player, if
   * known. <code>HasHighSpenderProbability()</code> must return true for this
   * function to be usable.
   */
  float HighSpenderProbability() const;

  /**
   * Returns whether or not this PlayerStats object has number of purchases
   * available. It must return true for <code>NumberOfPurchases()</code> to be
   * usable.
   */
  bool HasNumberOfPurchases() const;

  /**
   * Retrieves the number of purchases for this player, if known.
   * <code>HasNumberOfPurchases()</code> must return true for this function to
   * be usable.
   */
  int32_t NumberOfPurchases() const;

  /**
   * Returns whether or not this PlayerStats object has number of sessions
   * available. It must return true for <code>NumberOfSessions()</code> to be
   * usable.
   */
  bool HasNumberOfSessions() const;

  /**
   * Retrieves the number of sessions for this player, if known.
   * <code>HasNumberOfSessions()</code> must return true for this function to
   * be usable.
   */
  int32_t NumberOfSessions() const;

  /**
   * Returns whether or not this PlayerStats object has session percentile
   * available. It must return true for <code>SessionPercentile()</code> to be
   * usable.
   */
  bool HasSessionPercentile() const;

  /**
   * Retrieves the session percentile information for this player, if known.
   * <code>HasSessionPercentile()</code> must return true for this function
   * to be usable.
   */
  float SessionPercentile() const;

  /**
   * Returns whether or not this PlayerStats object has spend percentile
   * available. It must return true for <code>SpendPercentile()</code> to be
   * usable.
   */
  bool HasSpendPercentile() const;

  /**
   * Retrieves the spend percentile information for this player, if known.
   * <code>HasSpendPercentile()</code> must return true for this function
   * to be usable.
   */
  float SpendPercentile() const;

  /**
   * Returns whether or not this PlayerStats object has spend probability
   * available. It must return true for <code>SpendProbability()</code> to be
   * usable.
   */
  bool HasSpendProbability() const;

  /**
   * Retrieves the spend probability information for this player, if known.
   * <code>HasSpendProbability()</code> must return true for this function
   * to be usable.
   */
  float SpendProbability() const;

  /**
   * Returns whether or not this PlayerStats object has total spend over next
   * 28 days available. It must return true for
   * <code>TotalSpendNext28Days()</code> to be usable.
   */
  bool HasTotalSpendNext28Days() const;

  /**
   * Retrieves the total spend over next 28 days information for this player, if
   * known. <code>HasTotalSpendNext28Days()</code> must return true for this
   * function to be usable.
   */
  float TotalSpendNext28Days() const;

 private:
  friend class PlayerStatsImpl;
  std::shared_ptr<PlayerStatsImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_PLAYERSTATS_H_
