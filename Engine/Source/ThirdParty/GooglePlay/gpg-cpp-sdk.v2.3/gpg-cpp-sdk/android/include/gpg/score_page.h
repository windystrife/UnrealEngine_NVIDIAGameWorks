// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/score_page.h
 *
 * @brief Value object that represents a single page of high scores.
 */

#ifndef GPG_SCORE_PAGE_H_
#define GPG_SCORE_PAGE_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "gpg/common.h"
#include "gpg/score.h"
#include "gpg/types.h"

namespace gpg {

class EntryImpl;
class ScorePageImpl;
class ScorePageTokenImpl;

/**
 * A single data structure which allows you to access score data.
 * Data include Leaderboard id, start, timespan, collection, previous
 * score-page token, next score-page token, and the vector of all
 * score entries.
 * @ingroup ValueType
 */
class GPG_EXPORT ScorePage {
 public:
  /**
   * A class that creates an entry on a score page.
   */
  class Entry {
   public:
    Entry();

    /**
     * Explicit constructor.
     */
    explicit Entry(std::shared_ptr<EntryImpl const> impl);

    /**
     * Copy constructor for copying an existing entry into a new one.
     */
    Entry(Entry const &copy_from);

    /**
     * Constructor for moving an existing entry into a new one.
     * r-value-reference version.
     */
    Entry(Entry &&move_from);

    /**
     * Assignment operator for assigning this entry's value from another
     * entry.
     */
    Entry &operator=(Entry const &copy_from);

    /**
     * Assignment operator for assigning this entry's value from another
     * entry.
     * r-value-reference version.
     */
    Entry &operator=(Entry &&move_from);
    ~Entry();

    /**
     * Returns true when the returned entry is populated with data and is
     * accompanied by a successful response status; false for an
     * unpopulated user-created entry or for a populated one accompanied
     * by an unsuccessful response status.
     * It must be true for the getter functions on this entry (PlayerId,
     * Score, etc.) to be usable.
     */
    bool Valid() const;

    /**
     * Returns player ID.
     */
    std::string const &PlayerId() const;

    /**
     * Returns player score.
     */
    gpg::Score const &Score() const;

    /**
     * Returns time at which the entry was last modified (expressed as
     * milliseconds since the Unix epoch).
     */
    Timestamp LastModifiedTime() const;

    /**
     * @deprecated Prefer LastModifiedTime.
     */
    Timestamp LastModified() const GPG_DEPRECATED { return LastModifiedTime(); }

   private:
    std::shared_ptr<EntryImpl const> impl_;
  };

  /**
   * A data structure that is a nearly-opaque type representing a query for a
   * ScorePage (or is empty).
   * ScorePageToken is used in various Leaderboard functions that allow paging
   * through pages of scores.
   * Tokens created by this function will always start at the beginning of the
   * requested range.
   * The client may obtain a token either from a Leaderboard, in which case it
   * represents a query for the initial page of results for that query, or from
   * a previously-obtained ScorePage, in which case it represents a continuation
   * (paging) of that query.
   * @ingroup ValueType
   */
  class ScorePageToken {
   public:
    ScorePageToken();

    /**
     * Explicit constructor.
     */
    explicit ScorePageToken(std::shared_ptr<ScorePageTokenImpl const> impl);

    /**
     * Copy constructor for copying an existing score page token into a new one.
     */
    ScorePageToken(ScorePageToken const &copy_from);

    /**
     * Constructor for moving an existing score page token into a new one.
     * r-value-reference version.
     */
    ScorePageToken(ScorePageToken &&move_from);

    /**
     * Assignment operator for assigning this score page token's value from
     * another score page token.
     */
    ScorePageToken &operator=(ScorePageToken const &copy_from);

    /**
     * Assignment operator for assigning this score page token's value from
     * another score page token.
     * r-value-reference version.
     */
    ScorePageToken &operator=(ScorePageToken &&move_from);
    ~ScorePageToken();

    /**
     * Returns true when the returned score page token is populated with data
     * and is accompanied by a successful response status; false for an
     * unpopulated user-created token or for a populated one accompanied by
     * an unsuccessful response status.
     * It must be true for the getter functions on this token (LeaderboardId,
     * Start, etc.) to be usable.
     */
    bool Valid() const;

   private:
    friend class ScorePageTokenImpl;
    std::shared_ptr<ScorePageTokenImpl const> impl_;
  };

  ScorePage();

  /**
   * Explicit constructor.
   */
  explicit ScorePage(std::shared_ptr<ScorePageImpl const> impl);

  /**
   * Copy constructor for copying an existing score page into a new one.
   */
  ScorePage(ScorePage const &copy_from);

  /**
   * Constructor for moving an existing score page into a new one.
   * r-value-reference version.
   */
  ScorePage(ScorePage &&move_from);

  /**
   * Assignment operator for assigning this score page's value from another
   * score page.
   */
  ScorePage &operator=(ScorePage const &copy_from);

  /**
   * Assignment operator for assigning this score page's value from another
   * score page.
   * r-value-reference version.
   */
  ScorePage &operator=(ScorePage &&move_from);
  ~ScorePage();

  /**
   * Returns true if this <code>ScorePage</code> is populated with data.
   * Must return true for the getter functions on the
   * <code>ScorePage</code> object (<code>LeaderboardId</code>,
   * <code>Start</code>, etc...) to be usable.
   */
  bool Valid() const;

  // Properties that define this score page

  /**
   * Returns the unique string that the Google Play Developer Console generated
   * beforehand. Use it to refer to a leaderboard in your game client.
   * It can only be called when Leaderboard::Valid() returns true.
   */
  std::string const &LeaderboardId() const;

  /**
   * Returns whether the leaderboard was initially queried for top scores
   * or scores near the current player.
   * Possible values are TOP_SCORES and PLAYER_CENTERED.
   */
  LeaderboardStart Start() const;

  /**
   * Returns the timespan of the leaderboard.
   * Possible values are DAILY, WEEKLY, and ALL_TIME.
   */
  LeaderboardTimeSpan TimeSpan() const;

  /**
   * Returns whether the leaderboard is PUBLIC or SOCIAL.
   */
  LeaderboardCollection Collection() const;

  // Token methods

  /**
   * Valid if the scoreboard has a previous score page.
   */
  bool HasPreviousScorePage() const;

  /**
   * Valid if the scoreboard has a subsequent score page.
   */
  bool HasNextScorePage() const;

  /**
   * Returns the score-page token for the previous page.
   */
  ScorePage::ScorePageToken PreviousScorePageToken() const;

  /**
   * Returns the score-page token for the subsequent page.
   */
  ScorePage::ScorePageToken NextScorePageToken() const;

  // Score data
  /**
   * Vector of all score entries.
   */
  std::vector<ScorePage::Entry> const &Entries() const;

 private:
  friend class ScorePageImpl;
  std::shared_ptr<ScorePageImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_SCORE_PAGE_H_
