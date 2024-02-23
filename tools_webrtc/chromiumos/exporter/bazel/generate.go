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
	"bytes"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"sort"
	"strings"

	"github.com/afq984/webrtcimport/bazel/repr"
	"github.com/afq984/webrtcimport/misc"
	"github.com/afq984/webrtcimport/must"
	"github.com/bazelbuild/buildtools/labels"
)

func (ws *Workspace) Generate(root string) {

	ws.resolveSources()

	for _, p := range ws.packages {
		ws.generatePackage(root, p)
	}

	fromRoot := "/usr/local/google/home/aaronyu/webrtc/src"
	ws.addMiscFiles(fromRoot, root)
	ws.copySources(fromRoot, root)
}

// Find which package source belongs to.
// packageDirs is a list of package directories that end with "/".
func resolveSource(packageDirs []string, source string) labels.Label {
	// Given a list of lexicographically sorted package paths that end with "/",
	// the insertion point of a file is right after the package it belongs.
	i := sort.SearchStrings(packageDirs, source)
	if i == 0 {
		return resolveOrphanSource(source)
	}
	pkg := packageDirs[i-1]
	path, ok := strings.CutPrefix(source, pkg)
	if !ok {
		return resolveOrphanSource(source)
	}

	return labels.Label{
		Package: strings.TrimRight(pkg, "/"),
		Target:  path,
	}
}

// Returns the label that references the source, which does not belong to any package.
func resolveOrphanSource(source string) labels.Label {
	dir, name := path.Split(source)
	dir = strings.TrimRight(dir, "/")
	return labels.Label{Package: dir, Target: name}
}

// resolveSources figures out which package each source belong to.
func (ws *Workspace) resolveSources() {
	var packages []string
	for p := range ws.packages {
		packages = append(packages, p+"/")
	}
	sort.Strings(packages)

	for _, p := range ws.packages {
		for _, t := range p.targets {
			for _, source := range t.raw_sources.items() {
				label := resolveSource(packages, source.value)

				// Export the file if it does not belong to the same package.
				if label.Package != p.name {
					exportingPkg := ws.pkg(label.Package)
					exportingPkg.exportFiles[label.Target] = true
				}

				if strings.HasSuffix(source.value, ".h") {
					t.hdrs.appendCondition(source.key, label)
				} else {
					t.srcs.appendCondition(source.key, label)
				}
			}
		}
	}
}

func (ws *Workspace) generatePackage(root string, p *Package) {
	pDir := filepath.Join(root, p.name)
	must.Must0(os.MkdirAll(pDir, 0755))

	w := bytes.NewBuffer(nil)

	io.WriteString(w, `# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//:defs.bzl", "COPTS", "AVX2_COPTS", "require_condition")

package(
    default_visibility = ["//:__subpackages__"],
)

buildozer_template(
    name = "avx2_template",
    copts = COPTS + AVX2_COPTS,
    target_compatible_with = ["@platforms//cpu:x86_64"],
)

buildozer_template(
    name = "neon_template",
    target_compatible_with = require_condition("//:neon_build"),
)
`)

	if len(p.exportFiles) > 0 {
		fmt.Fprintf(w, `
exports_files(%s)
`, repr.Strings(
			func() []string {
				var files []string
				for k := range p.exportFiles {
					files = append(files, k)
				}
				return files
			}()))
	}

	var targets []*Target
	for _, t := range p.targets {
		targets = append(targets, t)
	}
	sort.Slice(targets, func(i, j int) bool { return targets[i].label.Target < targets[j].label.Target })

	for _, t := range targets {
		must.Must1(fmt.Fprintf(w, "\n%s(\n", t.Type))
		fmt.Fprintf(w, "    name = %s,\n", repr.String(t.label.Target))
		if !t.Deps.empty() {
			fmt.Fprintf(w, "    deps = %s,\n", t.Deps.render(func(ls []labels.Label) string { return repr.Labels(t.label.Package, ls) }))
		}
		if !t.hdrs.empty() {
			fmt.Fprintf(w, "    hdrs = %s,\n", t.hdrs.render(func(ls []labels.Label) string { return repr.SourceLabels(t.label.Package, ls) }))
		}
		if !t.srcs.empty() {
			fmt.Fprintf(w, "    srcs = %s,\n", t.srcs.render(func(ls []labels.Label) string { return repr.SourceLabels(t.label.Package, ls) }))
		}
		switch t.Type {
		case "cc_library", "cc_binary":
			io.WriteString(w, "    copts = COPTS,")
		}
		switch t.Label().Format() {
		case "//rtc_tools:unpack_aecdump":
			io.WriteString(w, `    visibility = ["//:__subpackages__", "@adhd//:__subpackages__"]`,)
		}
		io.WriteString(w, ")\n")
	}

	cmd := exec.Command("buildifier", "-type=build")
	cmd.Stdin = w
	output := must.Must1(cmd.Output())

	os.WriteFile(filepath.Join(pDir, "BUILD.bazel"), output, 0644)
}

func (ws *Workspace) copySources(fromRoot, toRoot string) {
	for _, s := range ws.sources {

		content, ok := ws.fileOverrides[s]
		if !ok {
			b := must.Must1(os.ReadFile(filepath.Join(fromRoot, s)))
			content = string(b)
		}

		content = strings.Replace(
			content,
			`#include "third_party/libevent/event.h"`,
			`#include <event.h>`,
			-1,
		)

		content = strings.Replace(
			content,
			`#include "base/third_party/libevent/event.h"`,
			`#include <event.h>`,
			-1,
		)

		content = strings.Replace(
			content,
			`#include "third_party/protobuf/src/google/protobuf`,
			`#include "google/protobuf`,
			-1,
		)

		dst := filepath.Join(toRoot, s)
		must.Must0(os.MkdirAll(filepath.Dir(dst), 0755))
		must.Must0(os.WriteFile(dst, []byte(content), 0644))
	}
}

func (ws *Workspace) AddBulidGN() {
	for pkg := range ws.packages {
		ws.sources = append(ws.sources, path.Join(pkg, "BUILD.gn"))
	}
}

func (ws *Workspace) addMiscFiles(fromRoot, toRoot string) {
	// From chromite:
	// https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:chromite/licensing/licenses_lib.py;l=70;drc=c7961f6047826e329d3d63d8ae9d1a95b79ff47f
	exprs := misc.Map([]string{
		`^(?i)copyright$`,
		`^(?i)copyright[.]txt$`,
		`^(?i)copyright[.]regex$`,
		`^(?i)copying.*$`,
		`^(?i)licen[cs]e.*$`,
		`^(?i)licensing.*$`,
		`^(?i)ipa_font_license_agreement_v1[.]0[.]txt$`,
		`^(?i)MIT-LICENSE$`,
		`^(?i)PKG-INFO$`,
	}, regexp.MustCompile)

	fromRootFs := os.DirFS(fromRoot)
	toCopy := map[string]bool{}
	for pkg := range ws.packages {
		if _, err := os.Stat(filepath.Join(fromRoot, pkg)); os.IsNotExist(err) {
			continue
		}
		must.Must0(fs.WalkDir(fromRootFs, pkg, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				log.Panic("error during walkdir: ", err)
			}

			// Don't recurse.
			if path != pkg && d.Type().IsDir() {
				return fs.SkipDir
			}

			basename := filepath.Base(path)
			if !d.Type().IsRegular() {
				return nil
			}
			if strings.HasPrefix(basename, "README") {
				toCopy[path] = true
				return nil
			}
			for _, expr := range exprs {
				if expr.MatchString(basename) {
					toCopy[path] = true
					return nil
				}
			}
			return nil
		}))
	}

	for fn := range toCopy {
		ws.sources = append(ws.sources, fn)
	}
}
