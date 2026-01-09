#!/bin/bash
# Fix Rust generated code to remove Eq and Hash traits
# These traits aren't compatible with google protobuf types (Timestamp, Duration)

set -e

RUST_GEN_FILE="gen/rust/star/v1/star.v1.rs"

if [ ! -f "$RUST_GEN_FILE" ]; then
    echo "Error: Generated file not found: $RUST_GEN_FILE"
    exit 1
fi

echo "Fixing Rust generated code..."

# Use perl for more reliable in-place editing
perl -i -pe '
  # Step 1: Remove Eq and Hash from all derives
  s/#\[derive\(Clone, Copy, PartialEq, Eq, Hash, ::prost::Message\)\]/#[derive(Clone, Copy, PartialEq, ::prost::Message)]/g;
  s/#\[derive\(Clone, PartialEq, Eq, Hash, ::prost::Message\)\]/#[derive(Clone, PartialEq, ::prost::Message)]/g;
  s/#\[derive\(Clone, PartialEq, Eq, Hash, ::prost::Oneof\)\]/#[derive(Clone, PartialEq, ::prost::Oneof)]/g;

  # Step 2: Add serde to specific BMS data structures
  # Match lines like "#[derive(...::prost::Message)]" followed by "pub struct Bms*Data {"
  if (/^#\[derive\(Clone, (Copy, )?PartialEq, ::prost::Message\)\]$/ && eof() != 1) {
    $_ = $_; # Keep current line
    $next_line = <>;
    if ($next_line =~ /^pub struct Bms.*Data \{$/) {
      # Add serde to this struct
      $_ =~ s/::prost::Message\)/::prost::Message, serde::Serialize, serde::Deserialize)/;
      $_ .= $next_line;
    } else {
      # Not a BMS data struct, output both lines as-is
      $_ .= $next_line;
    }
  }
' "$RUST_GEN_FILE"

echo "✓ Fixed $RUST_GEN_FILE"
echo "✓ Removed Eq and Hash traits"
echo "✓ Added serde to BMS data structures"
