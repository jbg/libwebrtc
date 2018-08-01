#!/bin/bash
function print_usage_and_exit {
  echo "Usage: $0 analyze-headers <dir>"
  echo "Usage: $0 add-sources <rename-file>"
  echo "Usage: $0 add-tests <rename-file>"
  exit 1
}

if [[ $# -lt 1 ]]; then
  print_usage_and_exit
fi

function process_rename_file {
  rename_file=$1
  process_function=$2
  IFS=$'\n'
  for rename_stanza in $(cat "$rename_file"); do
    echo "$rename_stanza"
    if [[ "$rename_stanza" =~ ^# ]]; then
      continue
    fi
    IFS=$' '
    arr=($rename_stanza)
    old_path=${arr[0]}
    new_path=${arr[2]}
    $process_function "$old_path" "$new_path"
  done
}

function process_associated_sources {
  old_path="$1"
  new_path="$2"
  if [[ "$old_path" =~ \.h$ ]]; then
    old_source_name="$(basename "$old_path" .h).cc"
    old_source_path_candidates=$(
      git grep --files-with-matches "^#include \"$old_path\"$" |
        grep "${old_source_name}$")
    if [[ $(echo "$old_source_path_candidates" | wc -w) -eq 1 ]]; then
      old_source_path="$old_source_path_candidates"
      new_source_path="$(dirname "$old_source_path")/$(basename "$new_path" .h).cc"
      echo "$old_source_path --> $new_source_path"
    fi
  fi
}

function process_associated_tests {
  old_path="$1"
  new_path="$2"
  if [[ "$old_path" =~ \.cc$ ]] && ! [[ "$old_path" =~ _unittest\.cc$ ]]; then
    old_test_name="$(basename "$old_path" .cc)_unittest.cc"
    old_test_path="$(dirname "$old_path")/$old_test_name"
    if [ -f "$old_test_path" ]; then
      new_test_path="$(dirname "$old_path")/$(basename "$new_path" .cc)_unittest.cc"
      echo "$old_test_path --> $new_test_path"
    fi
  fi
}

case "$1" in
"analyze-headers")
  dir=$2
  # Look through all header files.
  for header_file_path in $(find "$dir" -type f -name '*.h'); do
    # Extract the file name (without the .h).
    header_file_name=$(basename "$header_file_path" .h)
    
    # If there is an underscore already, assume it does not need to be renamed.
    if [[ "$header_file_name" =~ _ ]]; then
      continue
    fi
  
    # We need to know where to put the underscores, so try a heuristic:
    # 1. Look for any sequence in the file that matches (case insensitively) the
    #    file name.
    # 2. Remove any results which are either all lower case or all upper case
    #    (these aren't going to help).
    # 3. Convert the results (in camel case) into snake case.
    # 4. Deduplicate.
    #
    # If there is only one result then we're good: there's an unambiguous
    # translation in the file into snake case. Otherwise, we throw up our hands
    # and defer to a human.
    candidates=$(grep -io "$header_file_name" "$header_file_path" |
      grep -v "$header_file_name" |
      grep -v $(echo "$header_file_name" | tr '[:lower:]' '[:upper:]') |
      perl -pe 's/([A-Z][a-z])/_$1/g' |
      perl -pe 's/^_?//' |
      tr '[:upper:]' '[:lower:]' |
      sort |
      uniq)
  
    if [[ $(echo "$candidates" | wc -w) -eq 1 ]]; then
      # We only have one candidate: great! This is most likely correct.
      # If the candidate is the same as the file name, then no need to rename.
      if [ "$header_file_name" == "$candidates" ]; then
        continue
      fi
      echo "$header_file_path --> $(dirname "$header_file_path")/${candidates}.h"
    else
      # Either got 0 candidates or more than 1, need human intervention.
      echo "# TBD: $header_file_path"
    fi
  done
  ;;
"add-sources")
  process_rename_file "$2" process_associated_sources
  ;;
"add-tests")
  process_rename_file "$2" process_associated_tests
  ;;
*)
  print_usage_and_exit
  ;;
esac
