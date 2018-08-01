#!/bin/bash
# Run various operations according to the rename operations specified in the
# given rename file.
#
# The rename file must have the following format:
#
#   <old path 1> --> <new path 1>
#   <old path 2> --> <new path 2>
#
# For example:
#
#   a/old_name.h --> a/new_name.h
#   # Comments are allowed.
#   b/old.h --> b/new.h
#

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <move|install|update> <path to rename file>"
  exit 1
fi

cmd="$1"
rename_file="$2"

# Update the relevant BUILD.gn file with the renamed file.
function rename_in_build_file {
  old_file_path="$1"
  new_file_path="$2"
  old_file_name=$(basename "$old_file_path")
  new_file_name=$(basename "$new_file_path")
  dir_name=$(dirname "$old_file_path")
  # Look through parent directories for a BUILD.gn file.
  while [ "$dir_name" != "." ]; do
    build_file="${dir_name}/BUILD.gn"
    # Check that a BUILD.gn file exists and has the file in it.
    if [ -f "$build_file" ] && $(grep "$old_file_name" "$build_file" > /dev/null); then
      # Rename the old file to the new file.
      sed -i '' "s#${old_file_name}#${new_file_name}#" "$build_file"
      git add "$build_file"
      return 0
    else
      # Look in the parent directory.
      old_file_name="$(basename "$dir_name")/${old_file_name}"
      new_file_name="$(basename "$dir_name")/${new_file_name}"
      dir_name=$(dirname "$dir_name")
    fi
  done
  return 1
}

# Rename the file and update the relevant BUILD.gn file.
function rename_file {
  old_file_path="$1"
  new_file_path="$2"
  git mv "$old_file_path" "$new_file_path"
  echo "Moving $old_file_path to $new_file_path"
  if ! rename_in_build_file "$old_file_path" "$new_file_path"; then
    echo "[WARNING] Could not find BUILD.gn for $old_file_path"
  fi
}

# Rename a file related to the given header file by a change in suffix.
# E.g., related_file_suffix=.cc would update foo.cc --> foo_renamed.cc from
#     foo.h --> foo_renamed.h
function rename_related_file {
  old_header_path="$1"
  new_header_path="$2"
  related_file_suffix="$3"
  related_file_name="$(basename "$old_header_path" .h)${related_file_suffix}"
  related_file_path_candidates=$(git grep --files-with-matches "^#include \"$old_header_path\"$" | grep "${related_file_name}$")
  if [[ $(echo "$related_file_path_candidates" | wc -w) -eq 1 ]]; then
    related_file_path="$related_file_path_candidates"
  else
    return 1
  fi
  new_related_file_path="$(dirname "$related_file_path")/$(basename "$new_header_path" .h)${related_file_suffix}"
  rename_file "$related_file_path" "$new_related_file_path"
  return 0
}

# Echo out the header guard for a given file path.
# E.g., api/jsep.h turns into _API_JSEP_H_ .
function header_guard {
  echo "_${1}_" | perl -pe 's/[\/\.]/_/g' | perl -pe 's/(.)/\U$1/g'
}

IFS=$'\n'
for rename_stanza in $(cat "$rename_file" | grep -v '^#'); do
  IFS=$' '
  arr=($rename_stanza)
  old_header_path=${arr[0]}
  new_header_path=${arr[2]}
  case "$cmd" in
  "move")
    # Move the header file and any associated .cc and unittest file, updating
    # the relevant BUILD.gn files to point to the new paths.
    rename_file "$old_header_path" "$new_header_path"
    old_header_guard=$(header_guard "$old_header_path")
    new_header_guard=$(header_guard "$new_header_path")
    sed -i '' "s/${old_header_guard}/${new_header_guard}/g" "$new_header_path"
    git add "$new_header_path"
    rename_related_file "$old_header_path" "$new_header_path" ".cc"
    rename_related_file "$old_header_path" "$new_header_path" "_unittest.cc"
    ;;
  "install")
    # Generate forwarding headers for the old header path that include the new
    # header path.
    echo "Adding $old_header_path"
    cat << EOF > "$old_header_path"
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(steveanton): Remove this files once downstream projects have been
// updated to include the new path.

#include "$new_header_path"

EOF
    git add "$old_header_path"
    ;;
  "update")
    # Update all #include references from the old path to the new path.
    count=0
    IFS=$'\n'
    for includer_path in $(git grep --files-with-matches "^#include \"$old_header_path\"$"); do
      sed -i '' "s!#include \"$old_header_path\"!#include \"$new_header_path\"!g" "$includer_path"
      let count=count+1
    done
    echo "Updated $count includes from $old_header_path to $new_header_path"
    ;;
  *)
    echo "Unknown command: $cmd"
    exit 1
    ;;
  esac
done
