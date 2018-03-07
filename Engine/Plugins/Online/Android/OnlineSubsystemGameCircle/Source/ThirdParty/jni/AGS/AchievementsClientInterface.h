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
 * AchievementsClientInterface.h
 *
 * Client interface class for requesting and submitting information to the
 * Amazon Games Achievements service.
 */

#ifndef __ACHIEVEMENTS_CLIENT_INTERFACE_H_INCLUDED__
#define __ACHIEVEMENTS_CLIENT_INTERFACE_H_INCLUDED__

#include "AGSClientCommonInterface.h"

namespace AmazonGames {

    //************************************
    // Data access structures
    //************************************

    struct AchievementData {
        const char* id;
        const char* title;
        const char* description;
        const char* imageURL;
        int pointValue;
        bool isHidden;
        bool isUnlocked;
        float progress;
        int position;

        AchievementData()
            : id(0), title(0), description(0), imageURL(0)
        {}
    };

    struct AchievementsData {
        int numAchievements;
        const AchievementData* achievements;
    };

    struct UpdateProgressResponse {
        const char* achievementId;
        bool isNewlyUnlocked;
    };

    //***********************************
    // Callback classes
    //***********************************

    class IGetAchievementCb : public ICallback {
    public:
        virtual void onGetAchievementCb(
                ErrorCode errorCode,
                const AchievementData* responseStruct,
                int developerTag) = 0;
    };

    class IGetAchievementsCb : public ICallback {
    public:
        virtual void onGetAchievementsCb(
                ErrorCode errorCode,
                const AchievementsData* responseStruct,
                int developerTag) = 0;
    };

    class IUpdateProgressCb : public ICallback {
    public:
        virtual void onUpdateProgressCb(
                ErrorCode errorCode,
                const UpdateProgressResponse* responseStruct,
                int developerTag) = 0;
    };

    //***********************************
    // Handle classes
    //***********************************

    // All Handle classes have these functions:
    //    HandleStatus getHandleStatus();
    //    ErrorCode getErrorCode();
    //    int getDeveloperTag();

    class IGetAchievementHandle : public IHandle {
    public:
        virtual const AchievementData* getResponseData() = 0;

        virtual IGetAchievementHandle* clone() const = 0;
    };

    class IGetAchievementsHandle : public IHandle {
    public:
        virtual const AchievementsData* getResponseData() = 0;

        virtual IGetAchievementsHandle* clone() const = 0;
    };

    class IUpdateProgressHandle : public IHandle {
    public:
        virtual const UpdateProgressResponse* getResponseData() = 0;

        virtual IUpdateProgressHandle* clone() const = 0;
    };

    //************************************
    // Achievements Client Interface
    //************************************

    class AchievementsClientInterface {

    public:

        /**
         * Brings up the Amazon Games Achievements overlay for the player.
         */
        static void showAchievementsOverlay();

        //************************************
        // Callbacks
        //************************************

        static void getAchievements(
                IGetAchievementsCb* const callback,
                int developerTag = 0);

        static void getAchievementsForPlayer(
                const char* playerId,
                IGetAchievementsCb* const callback,
                int developerTag = 0);

        static void getAchievement(
                const char* achievementId,
                IGetAchievementCb* const callback,
                int developerTag = 0);

        static void getAchievementForPlayer(
                const char* achievementId,
                const char* playerId,
                IGetAchievementCb* const callback,
                int developerTag = 0);

        static void updateProgress(
                const char* achievementId,
                float percentComplete,
                IUpdateProgressCb* const callback,
                int developerTag = 0);

        //************************************
        // Handles
        //     HandleWrapper class is defined in AGSClientCommonInterface.h
        //************************************

        static HandleWrapper<IGetAchievementsHandle> getAchievements(
                int developerTag = 0);

        static HandleWrapper<IGetAchievementsHandle> getAchievementsForPlayer(
                const char* playerId,
                int developerTag = 0);

        static HandleWrapper<IGetAchievementHandle> getAchievement(
                const char* achievementId,
                int developerTag = 0);

        static HandleWrapper<IGetAchievementHandle> getAchievementForPlayer(
                const char* achievementId,
                const char* playerId,
                int developerTag = 0);

        static HandleWrapper<IUpdateProgressHandle> updateProgress(
                const char* achievementId,
                float percentComplete,
                int developerTag = 0);
    };
}

#endif
