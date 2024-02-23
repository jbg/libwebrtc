/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package main

import (
	"flag"
	"log"
	"os"
	"path/filepath"
	"strings"

	"github.com/bazelbuild/buildtools/labels"

	"webrtc.googlesource.com/src.git/tools_webrtc/chromiumos/exporter/bazel"
	"webrtc.googlesource.com/src.git/tools_webrtc/chromiumos/exporter/gn"
	"webrtc.googlesource.com/src.git/tools_webrtc/chromiumos/exporter/misc"
	"webrtc.googlesource.com/src.git/tools_webrtc/chromiumos/exporter/must"
)

func resolve(l labels.Label) (labels.Label, bool) {
	if after, found := strings.CutPrefix(l.Package, "third_party/abseil-cpp/"); found {
		l.Repository = "com_google_absl"
		l.Package = after
		return l, false
	}
	if l.Package == "third_party/protobuf" {
		return labels.Label{}, false
	}
	if l.Package == "third_party/libevent" || l.Package == "base/third_party/libevent" {
		return labels.Label{
			Repository: "pkg_config",
			Package:    "libevent",
			Target:     "libevent",
		}, false
	}
	if strings.HasPrefix(l.Package, "third_party") {
		switch l.Package {
		case "third_party/rnnoise", "third_party/pffft":
			return l, true
		}
		panic(l.Format())
	}
	switch l.Format() {
	case "//experiments:registered_field_trials",
		"//modules/audio_processing:audioproc_debug_proto":
		return l, false
	case "//:poison_default_task_queue":
		return labels.Label{}, false
	case "//build/config:executable_deps":
		return labels.Label{}, false
	}
	return l, true
}

func loadDesc(path string, rootLibs []string) *bazel.Workspace {
	f := must.Must1(os.Open(path))
	defer f.Close()
	gnTargets := must.Must1(gn.ParseDesc(f))

	queue := misc.Map(rootLibs, labels.Parse)
	queued := map[labels.Label]bool{}
	numBazelTargets := 0
	for _, l := range queue {
		queued[l] = true
	}

	ws := bazel.NewWorkspace()

	for len(queue) > 0 {
		numBazelTargets++
		label := queue[len(queue)-1]
		queue = queue[:len(queue)-1]

		gnTarget := gnTargets[label]
		target := ws.Target(label)

		for _, dep := range gnTarget.Deps {
			dep, handleDep := resolve(dep)
			if dep == (labels.Label{}) {
				continue
			}

			target.AppendDep(dep)

			if handleDep {
				if queued[dep] {
					continue
				}
				queued[dep] = true
				if dep.Repository == "" {
					// External dependency
					queue = append(queue, dep)
				}
			}

		}

		switch gnTarget.Type {
		case "source_set", "static_library":
			target.Type = "cc_library"
		case "executable":
			target.Type = "cc_binary"
		default:
			log.Panicf("%q %+v", label.Format(), gnTarget)
		}

		for _, source := range gnTarget.Sources {
			ws.AddSource(target, source)
		}
		for _, source := range gnTarget.Public {
			ws.AddSource(target, source)
		}
	}

	log.Printf("loaded %d Bazel targets out of %d GN targets", numBazelTargets, len(gnTargets))

	return ws
}

func main() {
	from := flag.String("from", "", "path of the source directory")
	to := flag.String("to", "", "path of the output directory")
	flag.Parse()
	if *from == "" {
		log.Panic("-from should not be empty")
	}
	if *to == "" {
		log.Panic("-to should not be empty")
	}

	rootLibs := []string{
		"//modules/audio_processing",
		"//api/audio:aec3_factory",
		"//api/task_queue:default_task_queue_factory",
		"//rtc_tools:unpack_aecdump",
	}
	wsX86 := loadDesc(filepath.Join(*from, "/desc.json"), rootLibs)
	wsArm := loadDesc(filepath.Join(*from, "/descarm.json"), rootLibs)
	ws := bazel.Merge(map[string]*bazel.Workspace{
		"@platforms//cpu:x86_64": wsX86,
		"//:neon_build":          wsArm,
	})

	ws.AddBulidGN()
	addWorkarounds(ws)
	ws.Generate(*from, *to)
}

func addWorkarounds(ws *bazel.Workspace) {
	t := ws.Target(labels.Parse("//experiments:registered_field_trials"))
	t.Type = "cc_library"
	ws.AddSourceOverride(t, "experiments/registered_field_trials.h", `// This file was automatically generated. Do not edit.

#ifndef GEN_REGISTERED_FIELD_TRIALS_H_
#define GEN_REGISTERED_FIELD_TRIALS_H_

#include "absl/strings/string_view.h"

namespace webrtc {

inline constexpr absl::string_view kRegisteredFieldTrials[] = {
    "",
};

}  // namespace webrtc

#endif  // GEN_REGISTERED_FIELD_TRIALS_H_
`)

	debugProto := ws.Target(labels.Parse("//modules/audio_processing:debug_proto"))
	debugProto.Type = "proto_library"
	ws.AddSource(debugProto, "modules/audio_processing/debug.proto")

	debugCCProto := ws.Target(labels.Parse("//modules/audio_processing:audioproc_debug_proto"))
	debugCCProto.Type = "cc_proto_library"
	debugCCProto.AppendDep(debugProto.Label())
}
