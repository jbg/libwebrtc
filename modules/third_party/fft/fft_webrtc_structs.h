/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_THIRD_PARTY_FFT_FFT_WEBRTC_STRUCTS_H_
#define MODULES_THIRD_PARTY_FFT_FFT_WEBRTC_STRUCTS_H_

#define MAXFFTSIZE 2048
#define NFACTOR 11

typedef struct {
  unsigned int SpaceAlloced;
  unsigned int MaxPermAlloced;
  double Tmp0[MAXFFTSIZE];
  double Tmp1[MAXFFTSIZE];
  double Tmp2[MAXFFTSIZE];
  double Tmp3[MAXFFTSIZE];
  int Perm[MAXFFTSIZE];
  int factor[NFACTOR];

} FFTstr;

#endif /* MODULES_THIRD_PARTY_FFT_FFT_WEBRTC_STRUCTS_H_ */
