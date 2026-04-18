#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./strip_second_column_time.sh [-i] <input_file> [output_file]

Removes the second whitespace-delimited column from each line.
Based on files like data/captures/log5.txt where columns are:
  <packet_no> <time> <details...>

Options:
  -i    Edit input file in place.

Examples:
  ./strip_second_column_time.sh data/captures/log5.txt data/captures/log5.no-time.txt
  ./strip_second_column_time.sh -i data/captures/log5.txt
EOF
}

inplace=0
if [[ "${1:-}" == "-i" ]]; then
  inplace=1
  shift
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

input_file="$1"
output_file="${2:-}"

if [[ ! -f "$input_file" ]]; then
  echo "Error: input file not found: $input_file" >&2
  exit 1
fi

# Keep only column 1 and columns 3+, then normalize output:
# - no leading spaces
# - exactly one space between column 1 and the remainder
# - remove trailing "[Packet size limited during capture]"
# - no trailing spaces
transform() {
  perl -pe '
    if (/^\s*(\S+)\s+\S+(?:\s+(.*))?\s*$/) {
      my ($first, $rest) = ($1, defined($2) ? $2 : "");
      $rest =~ s/^\s+//;
      $rest =~ s/\s*\[Packet size limited during capture\]\s*$//;
      $rest =~ s/\s+$//;
      $_ = $first . (length($rest) ? " $rest" : "") . "\n";
    } else {
      s/^\s+//;
      s/\s*\[Packet size limited during capture\]\s*$//;
      s/\s+$//;
    }
  ' "$1"
}

if [[ $inplace -eq 1 ]]; then
  tmp_file="$(mktemp)"
  trap 'rm -f "$tmp_file"' EXIT
  transform "$input_file" > "$tmp_file"
  mv "$tmp_file" "$input_file"
  trap - EXIT
  echo "Updated in place: $input_file"
  exit 0
fi

if [[ -n "$output_file" ]]; then
  transform "$input_file" > "$output_file"
  echo "Wrote: $output_file"
else
  transform "$input_file"
fi
