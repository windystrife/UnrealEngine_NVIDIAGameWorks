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
 * AGSClientCommonInterface.h
 *
 * Common header file for data types shared across the Client Interfaces
 */

#ifndef __AGS_CLIENT_COMMON_INTERFACE_H_INCLUDED__
#define __AGS_CLIENT_COMMON_INTERFACE_H_INCLUDED__

namespace AmazonGames {

    //***********************************
    // Constants
    //***********************************

    enum ErrorCode {
        /**
         * Indicates no error was encountered.
         */
        NO_ERROR=0,
        /**
         * Indicates an error by the Service that it could not recover from.
         */
        UNRECOVERABLE,
        /**
         * Indicates the Amazon Games Service is not bound and ready.
         */
        SERVICE_NOT_READY,
        /**
         * Indicates an IOException was thrown while attempting an operation
         */
        IO_ERROR,
        /**
         * Indicates a network connection error has occurred, most likely due to losing internet
         * connectivity.
         */
        CONNECTION_ERROR,
        /**
         * Indicates the device is not currently authenticated to perform an action.  This can be
         * avoided by checking AmazonGames.isAuthenticated()
         */
        AUTHENTICATION_ERROR,
        /**
         * General data validation error indicates data provided is not valid
         */
        DATA_VALIDATION_ERROR,
        /**
         * Unknown Error is when an error is given by the SDK that we do not have knowledge of
         */
        UNKNOWN_ERROR,
        /**
         * Error in JNI communication
         */
        JNI_ERROR,

    };

    enum HandleStatus {
        HANDLE_WAITING,
        HANDLE_SUCCESS,
        HANDLE_ERROR
    };

    //***********************************
    // Callback class
    //***********************************

    class ICallback {
    public:
        virtual ~ICallback();
    };

    //***********************************
    // Handle class
    //***********************************

    class IHandle {
    public:
        virtual ~IHandle();
        virtual HandleStatus getHandleStatus()=0;
        virtual ErrorCode getErrorCode()=0;
        virtual int getDeveloperTag()=0;
    };

    //***********************************
    // Handle Wrapper class
    //     This class wraps Handles returned by our API and handles clean up
    //     of the internal data.
    //***********************************

    template<typename T>
    class HandleWrapper {
    public:
        /**
         * Return the handle pointer
         */
        T* handle() {
            return m_Handle;
        }

        HandleWrapper() :
            m_Handle(0) { }

        HandleWrapper(T* handle) {
            m_Handle = handle;
        }
        ~HandleWrapper() {
            if (m_Handle) {
                delete m_Handle;
            }
        }
        HandleWrapper(const HandleWrapper &source) :
            m_Handle(0) {
            if (source.m_Handle) {
                m_Handle = source.m_Handle->clone();
            }
        }
        HandleWrapper& operator= (const HandleWrapper &source) {
            if (source.m_Handle) {
                m_Handle = source.m_Handle->clone();
            }
            return *this;
        }
    private:
        T* m_Handle;
    };
}

#endif

