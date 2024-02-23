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
	"log"
	"strings"

	"github.com/bazelbuild/buildtools/labels"
)

type Workspace struct {
	packages map[string]*Package
	sources  []string

	// TODO: Abstract on sources instead
	fileOverrides map[string]string
}

func NewWorkspace() *Workspace {
	return &Workspace{
		packages:      map[string]*Package{},
		fileOverrides: map[string]string{},
	}
}

func (ws *Workspace) pkg(name string) *Package {
	pkg := ws.packages[name]
	if pkg == nil {
		ws.packages[name] = &Package{
			name:        name,
			targets:     map[string]*Target{},
			exportFiles: make(map[string]bool),
		}
	}
	return ws.packages[name]
}

func (ws *Workspace) AddSource(target *Target, sourcePath string) {
	sourcePath = strings.Trim(sourcePath, "/")
	ws.sources = append(ws.sources, sourcePath)
	target.raw_sources.append(sourcePath)
}

func (ws *Workspace) AddSourceOverride(target *Target, path string, contents string) {
	path = strings.Trim(path, "/")
	ws.AddSource(target, path)
	ws.fileOverrides[path] = contents
}

func (ws *Workspace) Target(l labels.Label) *Target {
	if l.Repository != "" {
		log.Panicf("%q repository not empty", l.Format())
	}
	return ws.pkg(l.Package).target(l.Target)
}

type Package struct {
	name        string
	targets     map[string]*Target
	exportFiles map[string]bool
}

func (p *Package) target(name string) *Target {
	target := p.targets[name]
	if target == nil {
		p.targets[name] = &Target{
			label: labels.Label{
				Package: p.name,
				Target:  name,
			},
			raw_sources: newSelectableList[string](),
			hdrs:        newSelectableList[labels.Label](),
			srcs:        newSelectableList[labels.Label](),
			Deps:        newSelectableList[labels.Label](),
		}
	}
	return p.targets[name]
}

type Target struct {
	// Absolute label of the build target.
	label labels.Label
	// The type of the target, e.g. cc_library.
	Type string
	// Unresolved source list.
	raw_sources selectableList[string]
	// Resolved source list.
	hdrs selectableList[labels.Label]
	srcs selectableList[labels.Label]
	Deps selectableList[labels.Label]
}

func (t *Target) AppendDep(l labels.Label) {
	t.Deps.append(l)
}

func (t *Target) Label() labels.Label {
	return t.label
}
