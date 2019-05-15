/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/plot_protobuf.h"

#include <stddef.h>
#include <iostream>
#include <memory>
#include <vector>

namespace webrtc {

namespace {
webrtc::analytics::ChartId::Id ConvertChartId(ChartId id) {
  switch (id) {
    case ChartId::kNone:
      return webrtc::analytics::ChartId::kNone;
    case ChartId::kIncomingPacketSizes:
      return webrtc::analytics::ChartId::kIncomingPacketSizes;
    case ChartId::kOutgoingPacketSizes:
      return webrtc::analytics::ChartId::kOutgoingPacketSizes;
    case ChartId::kIncomingRtcpTypes:
      return webrtc::analytics::ChartId::kIncomingRtcpTypes;
    case ChartId::kOutgoingRtcpTypes:
      return webrtc::analytics::ChartId::kOutgoingRtcpTypes;
    case ChartId::kAccumulatedIncomingPackets:
      return webrtc::analytics::ChartId::kAccumulatedIncomingPackets;
    case ChartId::kAccumulatedOutgoingPackets:
      return webrtc::analytics::ChartId::kAccumulatedOutgoingPackets;
    case ChartId::kAudioPlayout:
      return webrtc::analytics::ChartId::kAudioPlayout;
    case ChartId::kIncomingAudioLevel:
      return webrtc::analytics::ChartId::kIncomingAudioLevel;
    case ChartId::kOutgoingAudioLevel:
      return webrtc::analytics::ChartId::kOutgoingAudioLevel;
    case ChartId::kIncomingSequenceNumberDeltas:
      return webrtc::analytics::ChartId::kIncomingSequenceNumberDeltas;
    case ChartId::kIncomingNetworkDelay:
      return webrtc::analytics::ChartId::kIncomingNetworkDelay;
    case ChartId::kOutgoingPacketLoss:
      return webrtc::analytics::ChartId::kOutgoingPacketLoss;
    case ChartId::kTotalIncomingBitrate:
      return webrtc::analytics::ChartId::kTotalIncomingBitrate;
    case ChartId::kTotalOutgoingBitrate:
      return webrtc::analytics::ChartId::kTotalOutgoingBitrate;
    case ChartId::kIncomingStreamBitrate:
      return webrtc::analytics::ChartId::kIncomingStreamBitrate;
    case ChartId::kOutgoingStreamBitrate:
      return webrtc::analytics::ChartId::kOutgoingStreamBitrate;
    case ChartId::kIncomingBitrateAllocation:
      return webrtc::analytics::ChartId::kIncomingBitrateAllocation;
    case ChartId::kOutgoingBitrateAllocation:
      return webrtc::analytics::ChartId::kOutgoingBitrateAllocation;
    case ChartId::kGoogCcBweSimulation:
      return webrtc::analytics::ChartId::kGoogCcBweSimulation;
    case ChartId::kSendSideBweSimulation:
      return webrtc::analytics::ChartId::kSendSideBweSimulation;
    case ChartId::kReceiveSideBweSimulation:
      return webrtc::analytics::ChartId::kReceiveSideBweSimulation;
    case ChartId::kOutgoingNetworkDelay:
      return webrtc::analytics::ChartId::kOutgoingNetworkDelay;
    case ChartId::kCaptureToSendDelay:
      return webrtc::analytics::ChartId::kCaptureToSendDelay;
    case ChartId::kIncomingTimestamps:
      return webrtc::analytics::ChartId::kIncomingTimestamps;
    case ChartId::kOutgoingTimestamps:
      return webrtc::analytics::ChartId::kOutgoingTimestamps;
    case ChartId::kIncomingRtcpFractionLost:
      return webrtc::analytics::ChartId::kIncomingRtcpFractionLost;
    case ChartId::kOutgoingRtcpFractionLost:
      return webrtc::analytics::ChartId::kOutgoingRtcpFractionLost;
    case ChartId::kIncomingRtcpCumulativeLost:
      return webrtc::analytics::ChartId::kIncomingRtcpCumulativeLost;
    case ChartId::kOutgoingRtcpCumulativeLost:
      return webrtc::analytics::ChartId::kOutgoingRtcpCumulativeLost;
    case ChartId::kIncomingRtcpHighestSeqNumber:
      return webrtc::analytics::ChartId::kIncomingRtcpHighestSeqNumber;
    case ChartId::kOutgoingRtcpHighestSeqNumber:
      return webrtc::analytics::ChartId::kOutgoingRtcpHighestSeqNumber;
    case ChartId::kIncomingRtcpDelaySinceLastSr:
      return webrtc::analytics::ChartId::kIncomingRtcpDelaySinceLastSr;
    case ChartId::kOutgoingRtcpDelaySinceLastSr:
      return webrtc::analytics::ChartId::kOutgoingRtcpDelaySinceLastSr;
    case ChartId::kAudioEncoderTargetBitrate:
      return webrtc::analytics::ChartId::kAudioEncoderTargetBitrate;
    case ChartId::kAudioEncoderFrameLength:
      return webrtc::analytics::ChartId::kAudioEncoderFrameLength;
    case ChartId::kAudioEncoderLostPackets:
      return webrtc::analytics::ChartId::kAudioEncoderLostPackets;
    case ChartId::kAudioEncoderFec:
      return webrtc::analytics::ChartId::kAudioEncoderFec;
    case ChartId::kAudioEncoderDtx:
      return webrtc::analytics::ChartId::kAudioEncoderDtx;
    case ChartId::kAudioEncoderNumChannels:
      return webrtc::analytics::ChartId::kAudioEncoderNumChannels;
    case ChartId::kNetEqTiming:
      return webrtc::analytics::ChartId::kNetEqTiming;
    case ChartId::kNetEqExpandRate:
      return webrtc::analytics::ChartId::kNetEqExpandRate;
    case ChartId::kNetEqSpeechExpandRate:
      return webrtc::analytics::ChartId::kNetEqSpeechExpandRate;
    case ChartId::kNetEqAccelerateRate:
      return webrtc::analytics::ChartId::kNetEqAccelerateRate;
    case ChartId::kNetEqPreemptiveRate:
      return webrtc::analytics::ChartId::kNetEqPreemptiveRate;
    case ChartId::kNetEqPacketLossRate:
      return webrtc::analytics::ChartId::kNetEqPacketLossRate;
    case ChartId::kNetEqConcealmentEvents:
      return webrtc::analytics::ChartId::kNetEqConcealmentEvents;
    case ChartId::kNetEqPreferredBufferSize:
      return webrtc::analytics::ChartId::kNetEqPreferredBufferSize;
    case ChartId::kIcePairConfigs:
      return webrtc::analytics::ChartId::kIcePairConfigs;
    case ChartId::kIceConnectivityChecks:
      return webrtc::analytics::ChartId::kIceConnectivityChecks;
    case ChartId::kDtlsTransportState:
      return webrtc::analytics::ChartId::kDtlsTransportState;
    case ChartId::kDtlsWritableState:
      return webrtc::analytics::ChartId::kDtlsWritableState;
  }
}
}  // namespace

ProtobufPlot::ProtobufPlot() {}

ProtobufPlot::~ProtobufPlot() {}

void ProtobufPlot::Draw() {}

void ProtobufPlot::ExportProtobuf(webrtc::analytics::Chart* chart) {
  for (size_t i = 0; i < series_list_.size(); i++) {
    webrtc::analytics::DataSet* data_set = chart->add_data_sets();
    for (const auto& point : series_list_[i].points) {
      data_set->add_x_values(point.x);
    }
    for (const auto& point : series_list_[i].points) {
      data_set->add_y_values(point.y);
    }

    if (series_list_[i].line_style == LineStyle::kBar) {
      data_set->set_style(webrtc::analytics::ChartStyle::BAR_CHART);
    } else if (series_list_[i].line_style == LineStyle::kLine) {
      data_set->set_style(webrtc::analytics::ChartStyle::LINE_CHART);
    } else if (series_list_[i].line_style == LineStyle::kStep) {
      data_set->set_style(webrtc::analytics::ChartStyle::LINE_STEP_CHART);
    } else if (series_list_[i].line_style == LineStyle::kNone) {
      data_set->set_style(webrtc::analytics::ChartStyle::SCATTER_CHART);
    } else {
      data_set->set_style(webrtc::analytics::ChartStyle::UNDEFINED);
    }

    if (series_list_[i].point_style == PointStyle::kHighlight)
      data_set->set_highlight_points(true);

    data_set->set_label(series_list_[i].label);
  }

  chart->set_xaxis_min(xaxis_min_);
  chart->set_xaxis_max(xaxis_max_);
  chart->set_yaxis_min(yaxis_min_);
  chart->set_yaxis_max(yaxis_max_);
  chart->set_xaxis_label(xaxis_label_);
  chart->set_yaxis_label(yaxis_label_);
  chart->set_title(title_);
  chart->set_id(ConvertChartId(id_));
}

ProtobufPlotCollection::ProtobufPlotCollection() {}

ProtobufPlotCollection::~ProtobufPlotCollection() {}

void ProtobufPlotCollection::Draw() {
  webrtc::analytics::ChartCollection collection;
  ExportProtobuf(&collection);
  std::cout << collection.SerializeAsString();
}

void ProtobufPlotCollection::ExportProtobuf(
    webrtc::analytics::ChartCollection* collection) {
  for (const auto& plot : plots_) {
    // TODO(terelius): Ensure that there is no way to insert plots other than
    // ProtobufPlots in a ProtobufPlotCollection. Needed to safely static_cast
    // here.
    webrtc::analytics::Chart* protobuf_representation =
        collection->add_charts();
    static_cast<ProtobufPlot*>(plot.get())
        ->ExportProtobuf(protobuf_representation);
  }
}

Plot* ProtobufPlotCollection::AppendNewPlot() {
  Plot* plot = new ProtobufPlot();
  plots_.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace webrtc
