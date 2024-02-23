/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package gn

import (
	"encoding/json"
	"io"

	"github.com/bazelbuild/buildtools/labels"

	"webrtc.googlesource.com/src.git/tools_webrtc/chromiumos/exporter/misc"
)

type rawDescTarget struct {
	Deps     []string `json:"deps"`
	Testonly bool     `json:"testonly"`
	Type     string   `json:"type"`
	Sources  []string `json:"sources"`
	Public   public   `json:"public"`
}

type Target struct {
	Deps     []labels.Label
	Testonly bool
	Type     string
	Sources  []string
	Public   []string
}

type public struct {
	values []string
}

func (p *public) UnmarshalJSON(b []byte) error {
	if string(b) == `"*"` {
		return nil
	}
	return json.Unmarshal(b, &p.values)
}

func parseLabels(pkg string, strings []string) []labels.Label {
	return misc.Map(
		strings,
		func(s string) labels.Label {
			return labels.ParseRelative(s, pkg)
		},
	)
}

func ParseDesc(r io.Reader) (map[labels.Label]*Target, error) {
	result := make(map[string]*rawDescTarget)
	dec := json.NewDecoder(r)
	if err := dec.Decode(&result); err != nil {
		return nil, err
	}

	// Replace //pkg:pkg with //pkg.
	canonical := map[labels.Label]*Target{}
	for rawLabel, rawTarget := range result {
		label := labels.Parse(rawLabel)
		canonical[label] = &Target{
			Deps:     parseLabels(label.Package, rawTarget.Deps),
			Testonly: rawTarget.Testonly,
			Type:     rawTarget.Type,
			Sources:  rawTarget.Sources,
			Public:   rawTarget.Public.values,
		}
	}
	return canonical, nil
}
