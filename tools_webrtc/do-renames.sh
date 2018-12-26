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
  echo "Usage: $0 <move|update|install> <path to rename file>"
  exit 1
fi

cmd="$1"
rename_file="$2"

# Echo out the header guard for a given file path.
# E.g., api/jsep.h turns into _API_JSEP_H_ .
function header_guard {
  echo "${1}_" | perl -pe 's/[\/\.]/_/g' | perl -pe 's/(.)/\U$1/g'
}

# Moves the file using git.
function move_file {
  old_path="$1"
  new_path="$2"
  git mv "$old_path" "$new_path"
  echo "Moved $old_path to $new_path"
}

# Update the relevant BUILD.gn file with the renamed file.
function rename_in_build_file {
  old_file_path="$1"
  new_file_path="$2"
  old_file_name=$(basename "$old_file_path")
  # Allow new_file_path to be in a subdirectory of the old file path.
  new_file_name=$(echo "$new_file_path" | sed "s#$(dirname "$old_file_path")/##")
  dir_name=$(dirname "$old_file_path")
  # Look through parent directories for a BUILD.gn file.
  while [ "$dir_name" != "." ]; do
    build_file="${dir_name}/BUILD.gn"
    # Check that a BUILD.gn file exists and has the file in it.
    if [ -f "$build_file" ] && $(grep "\"$old_file_name\"" "$build_file" > /dev/null); then
      # Rename the old file to the new file.
      sed -i "s#\"${old_file_name}\"#\"${new_file_name}\"#" "$build_file"
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

# Update all #include references from the old header path to the new path.
function update_all_includes {
  old_header_path="$1"
  new_header_path="$2"
  count=0
  IFS=$'\n'
  for includer_path in $(git grep --files-with-matches "^#include \"$old_header_path\""); do
    sed -i "s!#include \"$old_header_path\"!#include \"$new_header_path\"!g" "$includer_path"
    let count=count+1
  done
  echo -n $count
}

# Updates BUILD.gn and (if header) the include guard and all #include
# references.
function update_file {
  old_path="$1"
  new_path="$2"
  echo -n "Processing $old_path --> $new_path ... "
  echo -n " build file ... "
  if rename_in_build_file "$old_path" "$new_path"; then
    echo -n done
  else
    echo -n failed
  fi
  if [[ "$old_path" == *.h ]]; then
    echo -n " header guard ... "
    old_header_guard=$(header_guard "$old_path")
    new_header_guard=$(header_guard "$new_path")
    sed -i "s/${old_header_guard}/${new_header_guard}/g" "$new_path"
    echo -n done
    echo -n " includes ... "
    update_all_includes "$old_path" "$new_path"
    echo -n " done"
  fi
  echo
}

# Generate forwarding headers for the old header path that include the new
# header path.
function install_file {
  old_path="$1"
  new_path="$2"
  if [[ "$old_path" == *.h ]]; then
    cat << EOF > "$old_path"
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(bugs.webrtc.org/10159): Remove this files once downstream projects have
// been updated to include the new path.

#include "$new_path"

EOF
    git add "$old_path"
    echo "Installed header at $old_path pointing to $new_path"
  fi
}

IFS=$'\n'
for rename_stanza in $(cat "$rename_file" | grep -v '^#'); do
  IFS=$' '
  arr=($rename_stanza)
  old_path=${arr[0]}
  new_path=${arr[2]}
  case "$cmd" in
  "move")
    move_file "$old_path" "$new_path"
    ;;
  "update")
    update_file "$old_path" "$new_path"
    ;;
  "install")
    install_file "$old_path" "$new_path"
    ;;
  *)
    echo "Unknown command: $cmd"
    exit 1
    ;;
  esac
done
