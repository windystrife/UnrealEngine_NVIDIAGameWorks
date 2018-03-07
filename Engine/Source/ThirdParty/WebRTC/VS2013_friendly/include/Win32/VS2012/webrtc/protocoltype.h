/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

 // Epic Games Note: This was distilled from .../src/webrtc/p2p/base/port.h to remove the need to include all of .../src/webrtc/p2p/...
 
#ifndef WEBRTC_PROTOCOLTYPE_H_
#define WEBRTC_PROTOCOLTYPE_H_

namespace cricket {
enum ProtocolType {
  PROTO_UDP,
  PROTO_TCP,
  PROTO_SSLTCP,
  PROTO_LAST = PROTO_SSLTCP
};
}  // namespace cricket

#endif  // WEBRTC_PROTOCOLTYPE_H_
