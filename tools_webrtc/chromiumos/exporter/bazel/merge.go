/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package bazel

import (
	"github.com/bazelbuild/buildtools/labels"
)

// Merge workspaces.
func Merge(workspaces map[string]*Workspace) *Workspace {
	var wss []conditioned[*Workspace]
	for k, v := range workspaces {
		wss = append(wss, conditioned[*Workspace]{k, v})
	}

	out := NewWorkspace()

	targetMerges := map[labels.Label][]conditioned[*Target]{}

	for _, cws := range wss {
		out.sources = mergeSlice(out.sources, cws.value.sources)

		for _, pkg := range cws.value.packages {
			for _, target := range pkg.targets {
				targetMerges[target.label] = append(
					targetMerges[target.label],
					conditioned[*Target]{key: cws.key, value: target},
				)
			}
		}
	}

	for label, targets := range targetMerges {
		outTarget := out.Target(label)
		outTarget.Type = targets[0].value.Type
		outTarget.raw_sources = conditionField(targets, func(t *Target) selectableList[string] { return t.raw_sources })
		outTarget.Deps = conditionField(targets, func(t *Target) selectableList[labels.Label] { return t.Deps })
	}

	return out
}

func conditionField[T comparable](cts []conditioned[*Target], fieldFunc func(*Target) selectableList[T]) selectableList[T] {
	var inputs []conditioned[[]T]
	for _, ct := range cts {
		inputs = append(inputs, conditioned[[]T]{ct.key, fieldFunc(ct.value).list})
	}
	return mergeSelect(inputs)
}

func mergeSlice[T comparable](to, from []T) []T {
	known := map[T]bool{}
	for _, v := range to {
		known[v] = true
	}
	for _, v := range from {
		if !known[v] {
			to = append(to, v)
			known[v] = true
		}
	}
	return to
}
