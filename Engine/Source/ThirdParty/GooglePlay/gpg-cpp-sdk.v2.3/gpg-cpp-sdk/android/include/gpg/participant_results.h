// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/participant_results.h
 *
 * @brief Value object that represents results for the participants of a
 *        TurnBasedMatch.
 */

#ifndef GPG_PARTICIPANT_RESULTS_H_
#define GPG_PARTICIPANT_RESULTS_H_

#include <string>
#include <vector>
#include "gpg/multiplayer_participant.h"
#include "gpg/types.h"

namespace gpg {

class ParticipantResultsImpl;

/**
 * A data structure containing data about the per-participant results for a
 * <code>TurnBasedMatch</code>.
 *
 * @ingroup ValueType
 */
class GPG_EXPORT ParticipantResults {
 public:
  ParticipantResults();

  /**
   * Constructs a <code>ParticipantResults</code> object from a
   * <code>shared_ptr</code> to a <code>ParticipantResultsImpl</code>. Intended
   * for internal use by the API.
   */
  explicit ParticipantResults(
      std::shared_ptr<ParticipantResultsImpl const> impl);

  /**
   * Creates a copy of an existing <code>ParticipantResults</code> object.
   */
  ParticipantResults(ParticipantResults const &copy_from);

  /**
   * Moves an existing <code>ParticipantResults</code> object.
   */
  ParticipantResults(ParticipantResults &&move_from);

  /**
   * Assigns this <code>ParticipantResults</code> object from another one.
   */
  ParticipantResults &operator=(ParticipantResults const &copy_from);

  /**
   * Assigns this <code>ParticipantResults</code> object by moving another one
   * into it.
   */
  ParticipantResults &operator=(ParticipantResults &&move_from);

  /**
   * Returns true if this <code>ParticipantResults</code> object is populated
   * with data. Must be true in order for the getter functions
   * (<code>PlaceForParticipant</code>, <code>MatchResultForParticipant</code>,
   * etc...) on this <code>ParticipantResults</code> object to be usable.
   */
  bool Valid() const;

  /**
   * Returns true if this <code>ParticipantResults</code> object has a
   * result for the given <code>MultiplayerParticipant</code>. {@link Valid}
   * must return true for this function to be usable.
   */
  bool HasResultsForParticipant(std::string const &participant_id) const;

  /**
   * Returns the placing of the specified participant within a
   * <code>TurnBasedMatch</code>. Note that not all participants may have
   * results; if {@link HasResultsForParticipant} does not return true, this
   * function will return 0 for a player who is not yet ranked.  {@link Valid}
   * must return true for this function to be usable.
   */
  uint32_t PlaceForParticipant(std::string const &participant_id) const;

  /**
   * Returns the <code>MatchResult</code> for the specified participant within a
   * <code>TurnBasedMatch</code>. Note that not all participants may have a
   * MatchResult; if {@link HasResultsForParticipant} does not return true, this
   * function will return <code>MatchResult::NONE</code>. {@link Valid} must
   * return true for this function to be usable.
   */
  MatchResult MatchResultForParticipant(
      std::string const &participant_id) const;

  /**
   * Creates a new <code>ParticipantResults</code> that contains all existing
   * results and the additional result data passed into this function. Note that
   * a result can only be set once per participant. Attempting to set more than
   * one result will log an error and leave the
   * <code>ParticipantResults</code> unmodified.  {@link Valid} must return
   * true for this function to be usable.
   *
   * @param participant_id The <code>MultiplayerParticipant</code> for which to
   *     add a result.
   * @param placing The placing for the participant within the match.
   * @param result The <code>MatchResult</code> for the participant within the
   *     match.
   */
  ParticipantResults WithResult(std::string const &participant_id,
                                uint32_t placing,
                                MatchResult result) const;

 private:
  friend class ParticipantResultsImpl;
  std::shared_ptr<ParticipantResultsImpl const> impl_;
};

}  // namespace gpg

#endif  // GPG_PARTICIPANT_RESULTS_H_
