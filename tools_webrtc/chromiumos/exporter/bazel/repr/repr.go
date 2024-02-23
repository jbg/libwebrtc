/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package repr

import (
	"fmt"
	"sort"
	"strings"

	"github.com/afq984/webrtcimport/misc"
	"github.com/bazelbuild/buildtools/labels"
	"go.starlark.net/syntax"
)

func String(s string) string {
	return syntax.Quote(s, false)
}

func Strings(ss []string) string {
	sort.Strings(ss)
	return fmt.Sprintf("[%s]", strings.Join(misc.Map(ss, String), ", "))
}

func Label(from string, l labels.Label) string {
	return String(l.FormatRelative(from))
}

func Labels(from string, ls []labels.Label) string {
	return fmt.Sprintf(
		"[%s]",
		strings.Join(
			misc.Map(
				ls,
				func(l labels.Label) string { return Label(from, l) }),
			", ",
		),
	)
}

// Like Label but trims ":". Gives "example.cc" instead of ":example.cc".
func SourceLabel(from string, l labels.Label) string {
	return String(strings.TrimLeft(l.FormatRelative(from), ":"))
}

// Like Labels but trims ":". Gives "example.cc" instead of ":example.cc".
func SourceLabels(from string, ls []labels.Label) string {
	return fmt.Sprintf(
		"[%s]",
		strings.Join(
			misc.Map(
				ls,
				func(l labels.Label) string { return SourceLabel(from, l) }),
			", ",
		),
	)
}
