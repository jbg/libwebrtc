/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

@class WebRTCConfiguration;
@class WebRTCDataChannel;
@class WebRTCDataChannelConfiguration;
@class WebRTCIceCandidate;
@class WebRTCMediaConstraints;
@class WebRTCMediaStream;
@class WebRTCMediaStreamTrack;
@class WebRTCPeerConnectionFactory;
@class WebRTCRtpReceiver;
@class WebRTCRtpSender;
@class WebRTCRtpTransceiver;
@class WebRTCRtpTransceiverInit;
@class WebRTCSessionDescription;
@class RTCStatisticsReport;
@class WebRTCLegacyStatsReport;

typedef NS_ENUM(NSInteger, RTCRtpMediaType);

NS_ASSUME_NONNULL_BEGIN

extern NSString *const kRTCPeerConnectionErrorDomain;
extern int const kRTCSessionDescriptionErrorCode;

/** Represents the signaling state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCSignalingState) {
  RTCSignalingStateStable,
  RTCSignalingStateHaveLocalOffer,
  RTCSignalingStateHaveLocalPrAnswer,
  RTCSignalingStateHaveRemoteOffer,
  RTCSignalingStateHaveRemotePrAnswer,
  // Not an actual state, represents the total number of states.
  RTCSignalingStateClosed,
};

/** Represents the ice connection state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCIceConnectionState) {
  RTCIceConnectionStateNew,
  RTCIceConnectionStateChecking,
  RTCIceConnectionStateConnected,
  RTCIceConnectionStateCompleted,
  RTCIceConnectionStateFailed,
  RTCIceConnectionStateDisconnected,
  RTCIceConnectionStateClosed,
  RTCIceConnectionStateCount,
};

/** Represents the combined ice+dtls connection state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCPeerConnectionState) {
  RTCPeerConnectionStateNew,
  RTCPeerConnectionStateConnecting,
  RTCPeerConnectionStateConnected,
  RTCPeerConnectionStateDisconnected,
  RTCPeerConnectionStateFailed,
  RTCPeerConnectionStateClosed,
};

/** Represents the ice gathering state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCIceGatheringState) {
  RTCIceGatheringStateNew,
  RTCIceGatheringStateGathering,
  RTCIceGatheringStateComplete,
};

/** Represents the stats output level. */
typedef NS_ENUM(NSInteger, RTCStatsOutputLevel) {
  RTCStatsOutputLevelStandard,
  RTCStatsOutputLevelDebug,
};

@class WebRTCPeerConnection;

RTC_OBJC_EXPORT
@protocol WebRTCPeerConnectionDelegate <NSObject>

/** Called when the SignalingState changed. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didChangeSignalingState:(RTCSignalingState)stateChanged;

/** Called when media is received on a new stream from remote peer. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection didAddStream:(WebRTCMediaStream *)stream;

/** Called when a remote peer closes a stream.
 *  This is not called when RTCSdpSemanticsUnifiedPlan is specified.
 */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection didRemoveStream:(WebRTCMediaStream *)stream;

/** Called when negotiation is needed, for example ICE has restarted. */
- (void)peerConnectionShouldNegotiate:(WebRTCPeerConnection *)peerConnection;

/** Called any time the IceConnectionState changes. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didChangeIceConnectionState:(RTCIceConnectionState)newState;

/** Called any time the IceGatheringState changes. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didChangeIceGatheringState:(RTCIceGatheringState)newState;

/** New ice candidate has been found. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didGenerateIceCandidate:(WebRTCIceCandidate *)candidate;

/** Called when a group of local Ice candidates have been removed. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didRemoveIceCandidates:(NSArray<WebRTCIceCandidate *> *)candidates;

/** New data channel has been opened. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didOpenDataChannel:(WebRTCDataChannel *)dataChannel;

/** Called when signaling indicates a transceiver will be receiving media from
 *  the remote endpoint.
 *  This is only called with RTCSdpSemanticsUnifiedPlan specified.
 */
@optional
/** Called any time the IceConnectionState changes following standardized
 * transition. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didChangeStandardizedIceConnectionState:(RTCIceConnectionState)newState;

/** Called any time the PeerConnectionState changes. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didChangeConnectionState:(RTCPeerConnectionState)newState;

- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didStartReceivingOnTransceiver:(WebRTCRtpTransceiver *)transceiver;

/** Called when a receiver and its track are created. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
        didAddReceiver:(WebRTCRtpReceiver *)rtpReceiver
               streams:(NSArray<WebRTCMediaStream *> *)mediaStreams;

/** Called when the receiver and its track are removed. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
     didRemoveReceiver:(WebRTCRtpReceiver *)rtpReceiver;

/** Called when the selected ICE candidate pair is changed. */
- (void)peerConnection:(WebRTCPeerConnection *)peerConnection
    didChangeLocalCandidate:(WebRTCIceCandidate *)local
            remoteCandidate:(WebRTCIceCandidate *)remote
             lastReceivedMs:(int)lastDataReceivedMs
               changeReason:(NSString *)reason;

@end

RTC_OBJC_EXPORT
@interface WebRTCPeerConnection : NSObject

/** The object that will be notifed about events such as state changes and
 *  streams being added or removed.
 */
@property(nonatomic, weak, nullable) id<WebRTCPeerConnectionDelegate> delegate;
/** This property is not available with RTCSdpSemanticsUnifiedPlan. Please use
 *  |senders| instead.
 */
@property(nonatomic, readonly) NSArray<WebRTCMediaStream *> *localStreams;
@property(nonatomic, readonly, nullable) WebRTCSessionDescription *localDescription;
@property(nonatomic, readonly, nullable) WebRTCSessionDescription *remoteDescription;
@property(nonatomic, readonly) RTCSignalingState signalingState;
@property(nonatomic, readonly) RTCIceConnectionState iceConnectionState;
@property(nonatomic, readonly) RTCPeerConnectionState connectionState;
@property(nonatomic, readonly) RTCIceGatheringState iceGatheringState;
@property(nonatomic, readonly, copy) WebRTCConfiguration *configuration;

/** Gets all RTCRtpSenders associated with this peer connection.
 *  Note: reading this property returns different instances of WebRTCRtpSender.
 *  Use isEqual: instead of == to compare WebRTCRtpSender instances.
 */
@property(nonatomic, readonly) NSArray<WebRTCRtpSender *> *senders;

/** Gets all RTCRtpReceivers associated with this peer connection.
 *  Note: reading this property returns different instances of WebRTCRtpReceiver.
 *  Use isEqual: instead of == to compare WebRTCRtpReceiver instances.
 */
@property(nonatomic, readonly) NSArray<WebRTCRtpReceiver *> *receivers;

/** Gets all RTCRtpTransceivers associated with this peer connection.
 *  Note: reading this property returns different instances of
 *  WebRTCRtpTransceiver. Use isEqual: instead of == to compare WebRTCRtpTransceiver
 *  instances.
 *  This is only available with RTCSdpSemanticsUnifiedPlan specified.
 */
@property(nonatomic, readonly) NSArray<WebRTCRtpTransceiver *> *transceivers;

- (instancetype)init NS_UNAVAILABLE;

/** Sets the PeerConnection's global configuration to |configuration|.
 *  Any changes to STUN/TURN servers or ICE candidate policy will affect the
 *  next gathering phase, and cause the next call to createOffer to generate
 *  new ICE credentials. Note that the BUNDLE and RTCP-multiplexing policies
 *  cannot be changed with this method.
 */
- (BOOL)setConfiguration:(WebRTCConfiguration *)configuration;

/** Terminate all media and close the transport. */
- (void)close;

/** Provide a remote candidate to the ICE Agent. */
- (void)addIceCandidate:(WebRTCIceCandidate *)candidate;

/** Remove a group of remote candidates from the ICE Agent. */
- (void)removeIceCandidates:(NSArray<WebRTCIceCandidate *> *)candidates;

/** Add a new media stream to be sent on this peer connection.
 *  This method is not supported with RTCSdpSemanticsUnifiedPlan. Please use
 *  addTrack instead.
 */
- (void)addStream:(WebRTCMediaStream *)stream;

/** Remove the given media stream from this peer connection.
 *  This method is not supported with RTCSdpSemanticsUnifiedPlan. Please use
 *  removeTrack instead.
 */
- (void)removeStream:(WebRTCMediaStream *)stream;

/** Add a new media stream track to be sent on this peer connection, and return
 *  the newly created WebRTCRtpSender. The WebRTCRtpSender will be associated with
 *  the streams specified in the |streamIds| list.
 *
 *  Errors: If an error occurs, returns nil. An error can occur if:
 *  - A sender already exists for the track.
 *  - The peer connection is closed.
 */
- (WebRTCRtpSender *)addTrack:(WebRTCMediaStreamTrack *)track streamIds:(NSArray<NSString *> *)streamIds;

/** With PlanB semantics, removes an WebRTCRtpSender from this peer connection.
 *
 *  With UnifiedPlan semantics, sets sender's track to null and removes the
 *  send component from the associated WebRTCRtpTransceiver's direction.
 *
 *  Returns YES on success.
 */
- (BOOL)removeTrack:(WebRTCRtpSender *)sender;

/** addTransceiver creates a new WebRTCRtpTransceiver and adds it to the set of
 *  transceivers. Adding a transceiver will cause future calls to CreateOffer
 *  to add a media description for the corresponding transceiver.
 *
 *  The initial value of |mid| in the returned transceiver is nil. Setting a
 *  new session description may change it to a non-nil value.
 *
 *  https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-addtransceiver
 *
 *  Optionally, an RtpTransceiverInit structure can be specified to configure
 *  the transceiver from construction. If not specified, the transceiver will
 *  default to having a direction of kSendRecv and not be part of any streams.
 *
 *  These methods are only available when Unified Plan is enabled (see
 *  WebRTCConfiguration).
 */

/** Adds a transceiver with a sender set to transmit the given track. The kind
 *  of the transceiver (and sender/receiver) will be derived from the kind of
 *  the track.
 */
- (WebRTCRtpTransceiver *)addTransceiverWithTrack:(WebRTCMediaStreamTrack *)track;
- (WebRTCRtpTransceiver *)addTransceiverWithTrack:(WebRTCMediaStreamTrack *)track
                                          init:(WebRTCRtpTransceiverInit *)init;

/** Adds a transceiver with the given kind. Can either be RTCRtpMediaTypeAudio
 *  or RTCRtpMediaTypeVideo.
 */
- (WebRTCRtpTransceiver *)addTransceiverOfType:(RTCRtpMediaType)mediaType;
- (WebRTCRtpTransceiver *)addTransceiverOfType:(RTCRtpMediaType)mediaType
                                       init:(WebRTCRtpTransceiverInit *)init;

/** Generate an SDP offer. */
- (void)offerForConstraints:(WebRTCMediaConstraints *)constraints
          completionHandler:(nullable void (^)(WebRTCSessionDescription *_Nullable sdp,
                                               NSError *_Nullable error))completionHandler;

/** Generate an SDP answer. */
- (void)answerForConstraints:(WebRTCMediaConstraints *)constraints
           completionHandler:(nullable void (^)(WebRTCSessionDescription *_Nullable sdp,
                                                NSError *_Nullable error))completionHandler;

/** Apply the supplied WebRTCSessionDescription as the local description. */
- (void)setLocalDescription:(WebRTCSessionDescription *)sdp
          completionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/** Apply the supplied WebRTCSessionDescription as the remote description. */
- (void)setRemoteDescription:(WebRTCSessionDescription *)sdp
           completionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/** Limits the bandwidth allocated for all RTP streams sent by this
 *  PeerConnection. Nil parameters will be unchanged. Setting
 * |currentBitrateBps| will force the available bitrate estimate to the given
 *  value. Returns YES if the parameters were successfully updated.
 */
- (BOOL)setBweMinBitrateBps:(nullable NSNumber *)minBitrateBps
          currentBitrateBps:(nullable NSNumber *)currentBitrateBps
              maxBitrateBps:(nullable NSNumber *)maxBitrateBps;

/** Start or stop recording an Rtc EventLog. */
- (BOOL)startRtcEventLogWithFilePath:(NSString *)filePath maxSizeInBytes:(int64_t)maxSizeInBytes;
- (void)stopRtcEventLog;

@end

@interface WebRTCPeerConnection (Media)

/** Create an WebRTCRtpSender with the specified kind and media stream ID.
 *  See RTCMediaStreamTrack.h for available kinds.
 *  This method is not supported with RTCSdpSemanticsUnifiedPlan. Please use
 *  addTransceiver instead.
 */
- (WebRTCRtpSender *)senderWithKind:(NSString *)kind streamId:(NSString *)streamId;

@end

@interface WebRTCPeerConnection (DataChannel)

/** Create a new data channel with the given label and configuration. */
- (nullable WebRTCDataChannel *)dataChannelForLabel:(NSString *)label
                                   configuration:(WebRTCDataChannelConfiguration *)configuration;

@end

typedef void (^RTCStatisticsCompletionHandler)(RTCStatisticsReport *);

@interface WebRTCPeerConnection (Stats)

/** Gather stats for the given WebRTCMediaStreamTrack. If |mediaStreamTrack| is nil
 *  statistics are gathered for all tracks.
 */
- (void)statsForTrack:(nullable WebRTCMediaStreamTrack *)mediaStreamTrack
     statsOutputLevel:(RTCStatsOutputLevel)statsOutputLevel
    completionHandler:(nullable void (^)(NSArray<WebRTCLegacyStatsReport *> *stats))completionHandler;

/** Gather statistic through the v2 statistics API. */
- (void)statisticsWithCompletionHandler:(RTCStatisticsCompletionHandler)completionHandler;

/** Spec-compliant getStats() performing the stats selection algorithm with the
 *  sender.
 */
- (void)statisticsForSender:(WebRTCRtpSender *)sender
          completionHandler:(RTCStatisticsCompletionHandler)completionHandler;

/** Spec-compliant getStats() performing the stats selection algorithm with the
 *  receiver.
 */
- (void)statisticsForReceiver:(WebRTCRtpReceiver *)receiver
            completionHandler:(RTCStatisticsCompletionHandler)completionHandler;

@end

NS_ASSUME_NONNULL_END
