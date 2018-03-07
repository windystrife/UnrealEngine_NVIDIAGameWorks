// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_REQUESTS_USER_H
#define OVR_REQUESTS_USER_H

#include "OVR_Types.h"
#include "OVR_Platform_Defs.h"

#include "OVR_UserAndRoomArray.h"
#include "OVR_UserArray.h"

/// \file
/// Overview:
/// User objects represent people in the real world; their hopes, their dreams, and their current presence information.
///
/// Verifying Identify:
/// You can pass the result of ovr_UserProof_Generate() and ovr_GetLoggedInUserID()
/// to your your backend. Your server can use our api to verify identity.
/// 'https://graph.oculus.com/user_nonce_validate?nonce=USER_PROOF&user_id=USER_ID&access_token=ACCESS_TOKEN'
///
/// NOTE: the nonce is only good for one check and then it's invalidated.
///
/// App-Scoped IDs:
/// To protect user privacy, users have a different ovrID across different applications. If you are caching them,
/// make sure that you're also restricting them per application.

/// Retrieve the user with the given ID. This might fail if the ID is invalid
/// or the user is blocked.
///
/// NOTE: Users will have a unique ID per application.
/// \param userID User ID retrieved with this application.
///
/// A message with type ::ovrMessage_User_Get will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUser().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_Get(ovrID userID);

/// Return an access token for this user, suitable for making REST calls
/// against graph.oculus.com.
///
/// A message with type ::ovrMessage_User_GetAccessToken will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type const char *.
/// Extract the payload from the message handle with ::ovr_Message_GetString().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetAccessToken();

/// Retrieve the currently signed in user. This call is available offline.
///
/// NOTE: This will not return the user's presence as it should always be
/// 'online' in your application.
///
/// NOTE: Users will have a unique ID per application.
///
/// A message with type ::ovrMessage_User_GetLoggedInUser will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUser().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetLoggedInUser();

/// Retrieve a list of the logged in user's friends.
///
/// A message with type ::ovrMessage_User_GetLoggedInUserFriends will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserArrayHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUserArray().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetLoggedInUserFriends();

/// Retrieve a list of the logged in user's friends and any rooms they might be
/// in.
///
/// A message with type ::ovrMessage_User_GetLoggedInUserFriendsAndRooms will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserAndRoomArrayHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUserAndRoomArray().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetLoggedInUserFriendsAndRooms();

/// Get the next page of entries
///
/// A message with type ::ovrMessage_User_GetNextUserAndRoomArrayPage will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserAndRoomArrayHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUserAndRoomArray().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetNextUserAndRoomArrayPage(ovrUserAndRoomArrayHandle handle);

/// Get the next page of entries
///
/// A message with type ::ovrMessage_User_GetNextUserArrayPage will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserArrayHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUserArray().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetNextUserArrayPage(ovrUserArrayHandle handle);

/// returns an ovrID which is unique per org. allows different apps within the
/// same org to identify the user.
/// \param userID to load the org scoped id of
///
/// A message with type ::ovrMessage_User_GetOrgScopedID will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrOrgScopedIDHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetOrgScopedID().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetOrgScopedID(ovrID userID);

/// Part of the scheme to confirm the identity of a particular user in your
/// backend. You can pass the result of ovr_User_GetUserProof() and a user ID
/// from ovr_User_Get() to your your backend. Your server can then use our api
/// to verify identity. 'https://graph.oculus.com/user_nonce_validate?nonce=USE
/// R_PROOF&user_id=USER_ID&access_token=ACCESS_TOKEN'
///
/// NOTE: The nonce is only good for one check and then it is invalidated.
///
/// A message with type ::ovrMessage_User_GetUserProof will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrUserProofHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetUserProof().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_GetUserProof();

#endif
