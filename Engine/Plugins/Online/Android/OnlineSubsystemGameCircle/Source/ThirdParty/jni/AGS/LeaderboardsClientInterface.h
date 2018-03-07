/**
 * © 2012-2013 Amazon Digital Services, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may not use this file except in compliance with the License. A copy
 * of the License is located at
 *
 * http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 */

/**
 * LeaderboardsClientInterface.h
 *
 * Client interface class for requesting and submitting information to the
 * Amazon Games Leaderboards service.
 */

#ifndef __LEADERBOARDS_CLIENT_INTERFACE_H_INCLUDED__
#define __LEADERBOARDS_CLIENT_INTERFACE_H_INCLUDED__

#include "AGSClientCommonInterface.h"

namespace AmazonGames {

    //************************************
    // Constants
    //************************************

    /**
     * LeaderboardFilter defines the different leaderboard filters the system supports
     */
    enum LeaderboardFilter {
        GLOBAL_ALL_TIME=0,
        GLOBAL_WEEK,
        GLOBAL_DAY,
        FRIENDS_ALL_TIME
    };

    //************************************
    // Data access structures
    //************************************

    struct LeaderboardInfo {
        const char* leaderboardId;
        const char* leaderboardName;
        const char* displayText;
        const char* imageURL;
        int scoreFormat;
    };

    struct LeaderboardsInfo {
        int numLeaderboards;
        const LeaderboardInfo* leaderboards;

        LeaderboardsInfo()
            : leaderboards(0)
        {}
    };

    struct LeaderboardScore {
        const char* playerAlias;
        long long scoreValue;
        const char* scoreString;
        int rank;
        const char* leaderboardString;

        LeaderboardScore()
            : playerAlias(0), scoreString(0), leaderboardString(0)
        {}
    };

    struct LeaderboardScores {
        LeaderboardInfo leaderboardInfo;
        int numScores;
        const LeaderboardScore* scores;

        LeaderboardScores()
            : numScores(0), scores(0)
        {}
    };

    struct LeaderboardsRankImp {
        bool globalAllTime;
        bool globalWeek;
        bool globalDay;
        bool friendsAllTime;
    };

    struct LeaderboardsNewRanks {
        int globalAllTime;
        int globalWeek;
        int globalDay;
        int friendsAllTime;
    };

    struct SubmitScoreResponse {
        LeaderboardsRankImp rankImp;
        LeaderboardsNewRanks newRanks;
    };

    struct PlayerScoreInfo {
        const char* leaderboardId;
        long long scoreValue;
        int rank;
    };

    struct PercentileItem {
        const char* playerAlias;
        long long scoreValue;
        int percentile;
    };

    struct LeaderboardPercentiles {
        LeaderboardInfo leaderboardInfo;
        int userIndex;
        int numPercentiles;
        const PercentileItem* percentiles;
    };

    //************************************
    // Callback classes
    // ICallback is located in AGSClientCommonInterface.h
    //************************************

    class ILeaderboardGetLbsCb : public ICallback {
    public:
        virtual void onGetLeaderboardsCb(
                ErrorCode errorCode,
                const LeaderboardsInfo* responseStruct,
                int developerTag) = 0;
    };

    class ILeaderboardGetPlayerScoreCb : public ICallback {
    public:
        virtual void onGetPlayerScoreCb(
                ErrorCode errorCode,
                const PlayerScoreInfo* response,
                int developerTag) = 0;
    };

    class ILeaderboardGetPercentilesCb : public ICallback {
    public:
        virtual void onGetPercentilesCb(
                ErrorCode errorCode,
                const LeaderboardPercentiles* response,
                int developerTag) = 0;
    };

    class ILeaderboardGetScoresCb : public ICallback {
    public:
        virtual void onGetScoresCb(
                ErrorCode errorCode,
                const LeaderboardScores* scoresResponse,
                int developerTag) = 0;
    };

    class ILeaderboardSubmitScoreCb : public ICallback {
    public:
        virtual void onSubmitScoreCb(
                ErrorCode errorCode,
                const SubmitScoreResponse* submitScoreResponse,
                int developerTag) = 0;
    };

    //************************************
    // Handle classes
    //************************************

    // All Handle classes have these functions:
    //    HandleStatus getHandleStatus();
    //    ErrorCode getErrorCode();
    //    int getDeveloperTag();

    class IGetLeaderboardsHandle : public IHandle {
    public:
        virtual const LeaderboardsInfo* getResponseData() = 0;

        virtual IGetLeaderboardsHandle* clone() const = 0;
    };

    class IGetPlayerScoreHandle : public IHandle {
    public:
        virtual const PlayerScoreInfo* getResponseData() = 0;

        virtual IGetPlayerScoreHandle* clone() const = 0;
    };

    class IGetPercentilesHandle : public IHandle {
    public:
        virtual const LeaderboardPercentiles* getResponseData() = 0;

        virtual IGetPercentilesHandle* clone() const = 0;
    };

    class IGetScoresHandle : public IHandle {
    public:
        virtual const LeaderboardScores* getResponseData() = 0;

        virtual IGetScoresHandle* clone() const = 0;
    };

    class ISubmitScoreHandle : public IHandle {
    public:
        virtual const SubmitScoreResponse* getResponseData() = 0;

        virtual ISubmitScoreHandle* clone() const = 0;
    };

    //************************************
    // Leaderboards Client Interface
    //************************************

    class LeaderboardsClientInterface {

    public:

        /**
         * Brings up the Amazon Games Leaderboards Overlay.
         */
        static void showLeaderboardsOverlay();

        /**
         * Bring up the overlay for a specific leaderboard
         */
        static void showLeaderboardOverlay(char const* const leaderboardId);

        //************************************
        // Callbacks
        //************************************

        /**
         * Request the leaderboards for your game.  The SDK will call your callback pointer
         * when the data is ready or if an error has occured.
         *
         * @param developerTag
         * @param callback
         */
        static void getLeaderboards(
                ILeaderboardGetLbsCb* const callback,
                int developerTag = 0);

        /**
         * @param leaderboardId
         * @param scope
         * @param developerTag
         * @param callback
         */
        static void getPlayerScore(
                char const* const leaderboardId,
                AmazonGames::LeaderboardFilter filter,
                ILeaderboardGetPlayerScoreCb* const callback,
                int developerTag = 0);

        static void getScoreForPlayer(
                char const* const leaderboardId,
                char const* const playerId,
                AmazonGames::LeaderboardFilter filter,
                ILeaderboardGetPlayerScoreCb* const callback,
                int developerTag = 0);

        static void getPercentileRanks(
                char const* const leaderboardId,
                AmazonGames::LeaderboardFilter filter,
                ILeaderboardGetPercentilesCb* const callback,
                int developerTag = 0);

        static void getPercentileRanksForPlayer(
                char const* const leaderboardId,
                char const* const playerId,
                AmazonGames::LeaderboardFilter filter,
                ILeaderboardGetPercentilesCb* const callback,
                int developerTag = 0);

        /**
         * @param leaderboardId
         * @param scope
         * @param developerTag
         * @param callback
         */
        static void getScores(
                char const* const leaderboardId,
                AmazonGames::LeaderboardFilter filter,
                ILeaderboardGetScoresCb* const callback,
                int developerTag = 0);

        /**
         * @param leaderboardId
         * @param score
         * @param developerTag
         * @param callback
         */
        static void submitScore(
                char const* const leaderboardId,
                long long score,
                ILeaderboardSubmitScoreCb* const callback,
                int developerTag = 0);

        //************************************
        // Handles
        //************************************

        static HandleWrapper<IGetLeaderboardsHandle> getLeaderboards(
                int developerTag = 0);

        static HandleWrapper<IGetPlayerScoreHandle> getPlayerScore(
                char const* const leaderboardId,
                AmazonGames::LeaderboardFilter filter,
                int developerTag = 0);

        static HandleWrapper<IGetPlayerScoreHandle> getScoreForPlayer(
                char const* const leaderboardId,
                char const* const playerId,
                AmazonGames::LeaderboardFilter filter,
                int developerTag = 0);

        static HandleWrapper<IGetPercentilesHandle> getPercentileRanks(
                char const* const leaderboardId,
                AmazonGames::LeaderboardFilter filter,
                int developerTag = 0);

        static HandleWrapper<IGetPercentilesHandle> getPercentileRanksForPlayer(
                char const* const leaderboardId,
                char const* const playerId,
                AmazonGames::LeaderboardFilter filter,
                int developerTag = 0);

        static HandleWrapper<IGetScoresHandle> getScores(
                char const* const leaderboardId,
                AmazonGames::LeaderboardFilter filter,
                int developerTag = 0);

        static HandleWrapper<ISubmitScoreHandle> submitScore(
                char const* const leaderboardId,
                long long score,
                int developerTag = 0);
    };
};

#endif
