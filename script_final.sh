#!/bin/bash

# Function to create files in a directory
create_files() {
  local size=$1
  local count=$2
  local directory=$3

  mkdir -p "$directory"
  for ((i = 1; i <= count; i++)); do
    dd if=/dev/zero of="$directory/file_$i" bs="$size" count=1 status=none
  done
}

# Function to create files and subdirectories recursively
create_recursive_files() {
  local size=$1
  local count=$2
  local directory=$3
  local depth=$4

  if [[ $depth -eq 0 ]]; then
    return
  fi

  mkdir -p "$directory"
  for ((i = 1; i <= count; i++)); do
    dd if=/dev/zero of="$directory/file_$i" bs="$size" count=1 status=none
  done

  for ((i = 1; i <= 10; i++)); do
    create_recursive_files "$size" "$count" "$directory/subdirectory_$i" "$((depth - 1))"
  done
}

# Measure time for Case 1: 100 files of 1GB each in directory1
time create_files 1G 100 directory1

# Measure time for Case 2: 5000 files of 10MB each, including subdirectories, in directory2
time create_recursive_files 10M 100 directory2 50

# Count the total number of files in directory2
total_files=$(find directory2 -type f | wc -l)
echo "Total files created in directory2: $total_files"
