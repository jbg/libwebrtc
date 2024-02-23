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
	"fmt"
	"sort"
	"strings"

	"webrtc.googlesource.com/src.git/tools_webrtc/chromiumos/exporter/bazel/repr"
)

type selectableList[T comparable] struct {
	list       []T
	selections map[string][]T
}

func newSelectableList[T comparable]() selectableList[T] {
	return selectableList[T]{
		selections: make(map[string][]T),
	}
}

func (sl *selectableList[T]) append(values ...T) {
	sl.list = append(sl.list, values...)
}

func (sl *selectableList[T]) appendCondition(condition string, value T) {
	if condition == "" {
		sl.append(value)
	} else {
		sl.selections[condition] = append(sl.selections[condition], value)
	}
}

func (sl *selectableList[T]) items() []conditioned[T] {
	var output []conditioned[T]
	for _, x := range sl.list {
		output = append(output, conditioned[T]{value: x})
	}
	for condition, values := range sl.selections {
		for _, x := range values {
			output = append(output, conditioned[T]{key: condition, value: x})
		}
	}
	return output
}

type conditioned[T any] struct {
	key   string
	value T
}

func mergeSelect[T comparable](inputs []conditioned[[]T]) selectableList[T] {
	intersection := sliceToMap(inputs[0].value)
	for _, input := range inputs[1:] {
		values := sliceToMap(input.value)
		var drop []T
		for k := range intersection {
			if !values[k] {
				drop = append(drop, k)
			}
		}
		for _, v := range drop {
			delete(intersection, v)
		}
	}

	output := selectableList[T]{
		selections: map[string][]T{},
	}

	for k := range intersection {
		output.list = append(output.list, k)
	}
	for _, input := range inputs {
		var sel []T
		for _, v := range input.value {
			if !intersection[v] {
				sel = append(sel, v)
			}
		}
		if len(sel) > 0 {
			output.selections[input.key] = sel
		}
	}

	return output
}

func (sl *selectableList[T]) empty() bool {
	return len(sl.list) == 0 && len(sl.selections) == 0
}

func (sl *selectableList[T]) render(formatter func([]T) string) string {
	var parts []string
	if len(sl.list) > 0 {
		parts = append(parts, formatter(sl.list))
	}
	var selections []conditioned[[]T]
	for cond, value := range sl.selections {
		selections = append(selections, conditioned[[]T]{cond, value})
	}
	sort.Slice(selections, func(i, j int) bool {
		return selections[i].key < selections[j].key
	})
	for _, sel := range selections {
		parts = append(parts,
			fmt.Sprintf(
				`select({%s: %s, "//conditions:default": %s})`,
				repr.String(sel.key),
				formatter(sel.value),
				formatter(nil),
			),
		)
	}
	return strings.Join(parts, " + ")
}

func sliceToMap[T comparable](slice []T) map[T]bool {
	result := map[T]bool{}
	for _, x := range slice {
		result[x] = true
	}
	return result
}
