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
 * GameCircleClientInterface.h
 *
 * Client interface class for general SDK features.
 */

#ifndef __GAMECIRCLE_CLIENT_INTERFACE_H_INCLUDED__
#define __GAMECIRCLE_CLIENT_INTERFACE_H_INCLUDED__

#include "AGSClientCommonInterface.h"

namespace AmazonGames {

    //***********************************
    // Callback classes
    //***********************************

    class IShowGameCircleCb : public ICallback {
    public:
        virtual void onShowGameCircleCb(
                ErrorCode errorCode,
                int developerTag) = 0;
    };

    class IShowSignInPageCb : public ICallback {
    public:
        virtual void onShowSignInPageCb(
                ErrorCode errorCode,
                int developerTag) = 0;
    };

    //***********************************
    // Handle classes
    //***********************************

    // All Handle classes have these functions:
    //    HandleStatus getHandleStatus();
    //    ErrorCode getErrorCode();
    //    int getDeveloperTag();

    class IShowGameCircleHandle : public IHandle {
    public:
        virtual IShowGameCircleHandle* clone() const = 0;
    };

    class IShowSignInPageHandle : public IHandle {
    public:
        virtual IShowSignInPageHandle* clone() const = 0;
    };

    class GameCircleClientInterface {

    public:

        //************************************
        // Callbacks
        //************************************

        /**
         * Show the Game Circle overlay.
         */
        static void showGameCircle(
                IShowGameCircleCb* const callback,
                int developerTag = 0);


        /**
         * Show the Game Circle sign in page.
         */
        static void showSignInPage(
                IShowSignInPageCb* const callback,
                int developerTag = 0);

        //************************************
        // Handles
        //     HandleWrapper class is defined in AGSClientCommonInterface.h
        //************************************

        /**
         * Show the Game Circle overlay.
         */
        static HandleWrapper<IShowGameCircleHandle> showGameCircle(
                int developerTag = 0);


        /**
         * Show the Game Circle sign in page.
         */
        static HandleWrapper<IShowSignInPageHandle> showSignInPage(
                int developerTag = 0);
    };
}

#endif
