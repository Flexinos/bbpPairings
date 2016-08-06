/*
 * This file is part of BBP Pairings, a Swiss-system chess tournament engine
 * Copyright (C) 2016  Jeremy Bierema
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.0, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <utility/typesizes.h>
#include <utility/uinttypes.h>

#ifndef MAX_PLAYERS
  #define MAX_PLAYERS 9999
#endif

#ifndef MAX_POINTS
  #define MAX_POINTS 1998
#endif

#ifndef MAX_RATING
  #define MAX_RATING 9999
#endif

namespace tournament
{
  /**
   * An exception indicating that the operation could not be completed because
   * the constants set in this file are too small.
   */
  struct BuildLimitExceededException : public std::logic_error
  {
    explicit BuildLimitExceededException(const std::string &explanation)
      : std::logic_error(explanation) { }
  };

  typedef utility::uinttypes::uint_least_for_value<MAX_PLAYERS> player_index;
  /**
   * A type representing a person's score, stored as ten times the actual score.
   */
  typedef utility::uinttypes::uint_least_for_value<MAX_POINTS> points;
  typedef utility::uinttypes::uint_least_for_value<MAX_RATING> rating;

  constexpr player_index maxPlayers = MAX_PLAYERS;
  constexpr points maxPoints = MAX_POINTS;

  enum Color : utility::uinttypes::uint_least_for_value<2>
  {
    COLOR_WHITE, COLOR_BLACK, COLOR_NONE
  };

  constexpr Color invert(const Color color)
  {
    return
      color == COLOR_WHITE ? COLOR_BLACK
        : color == COLOR_BLACK ? COLOR_WHITE
        : COLOR_NONE;
  }

  enum MatchScore : utility::uinttypes::uint_least_for_value<2>
  {
    MATCH_SCORE_LOSS, MATCH_SCORE_DRAW, MATCH_SCORE_WIN
  };

  constexpr MatchScore invert(const MatchScore matchScore)
  {
    return MatchScore(MATCH_SCORE_WIN - matchScore);
  }

  /**
   * A type representing the history of a single player on a single round.
   */
  struct Match
  {
    /**
     * The ID of the opponent. The lack of an opponent is indicated by using the
     * player's own ID.
     */
    player_index opponent;
    Color color;
    MatchScore matchScore;
    bool gameWasPlayed;
    /**
     * The player was either paired or given the pairing-allocated bye.
     */
    bool participatedInPairing;

    Match(const player_index playerIndex)
      : opponent(playerIndex),
        color(COLOR_NONE),
        matchScore(MATCH_SCORE_LOSS),
        gameWasPlayed(false),
        participatedInPairing(false)
    { }
    Match(
        const player_index opponent_,
        const Color color_,
        const MatchScore matchScore_,
        const bool gameWasPlayed_,
        const bool participatedInPairing_)
      : opponent(opponent_),
        color(color_),
        matchScore(matchScore_),
        gameWasPlayed(gameWasPlayed_),
        participatedInPairing(participatedInPairing_)
    { }
  };

  struct Player
  {
    std::vector<Match> matches;
    /**
     * Round-indexed accelerations. If the vector is shorter than the number of
     * rounds, zeroes are implied.
     */
    std::vector<points> accelerations;
    /**
     * The player may not be paired against these opponents.
     */
    std::unordered_set<player_index> forbiddenPairs;

    /**
     * The zero-indexed pairing ID used for input/output.
     */
    player_index id;
    /**
     * The effective pairing number for the current round, that is, the pairing
     * number used for choosing colors and for breaking ties.
     */
    player_index rankIndex;

    /**
     * Missing ratings are indicated by zeroes.
     */
    tournament::rating rating;

    points scoreWithoutAcceleration;
    /**
     * Acceleration for the current round.
     */
    points acceleration{ };

    Color colorPreference = COLOR_NONE;
    bool absoluteColorPreference{ };
    bool strongColorPreference{ };

    /**
     * The record corresponds to a player in the tournament, rather than a hole
     * in the player IDs.
     */
    bool isValid{ };

    Player() = default;
    Player(
        const player_index id_,
        const points points_,
        const tournament::rating rating_,
        std::vector<Match> &&matches_ = std::vector<Match>(),
        std::unordered_set<player_index> &&forbiddenPairs_ =
          std::unordered_set<player_index>())
      : matches(std::move(matches_)),
        forbiddenPairs(std::move(forbiddenPairs_)),
        id(id_),
        rankIndex(id_),
        rating(rating_),
        scoreWithoutAcceleration(points_),
        isValid(true)
    { }

    points scoreWithAcceleration() const
    {
      return scoreWithoutAcceleration + acceleration;
    }
  };

  /**
   * Compare the players based on current score, breaking ties using the
   * rankIndex.
   */
  inline bool unacceleratedScoreRankCompare(
    const Player *const player0,
    const Player *const player1)
  {
    return
      std::tie(player0->scoreWithoutAcceleration, player1->rankIndex)
        < std::tie(player1->scoreWithoutAcceleration, player0->rankIndex);
  }

  typedef
    utility::typesizes
      ::smallest<
#ifdef MAX_ROUNDS
        utility::uinttypes::uint_least_for_value<MAX_ROUNDS>,
#endif
        decltype(std::declval<Player>().matches)::size_type,
        std::string::size_type
      >::type
    round_index;
  constexpr round_index maxRounds =
#ifdef MAX_ROUNDS
    utility::typesizes::minUint(
      MAX_ROUNDS,
#endif
      std::numeric_limits<round_index>::max()
#ifdef MAX_ROUNDS
    )
#endif
    ;

  /**
   * A struct representing the details and history of a tournament.
   */
  struct Tournament
  {
    /**
     * Players indexed by ID.
     */
    std::vector<Player> players;
    /**
     * Players indexed by their effective pairing numbers, that is, the pairing
     * number used for choosing colors and breaking ties.
     */
    std::deque<player_index> playersByRank;
    round_index playedRounds{ };
    round_index expectedRounds{ };
    points pointsForWin = 10;
    points pointsForDraw = 5;
    Color initialColor = COLOR_NONE;

    points getPoints(const MatchScore matchScore) const &
    {
      return
        matchScore == MATCH_SCORE_LOSS ? 0
          : matchScore == MATCH_SCORE_WIN ? pointsForWin
          : pointsForDraw;
    }

    void forbidPairs(const std::deque<player_index> &) &;

    void updateRanks() &;
    void computePlayerData() &;
  };

  static_assert(
    maxPlayers
      <= std::numeric_limits<
            decltype(std::declval<Tournament>().players)::size_type
          >::max(),
    "Overfill");
}

#endif