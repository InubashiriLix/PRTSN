#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
echo "build scirpt dir at: ${script_dir}"
echo ""
echo "Building BLE Agent Client"

cargo build \
    --manifest-path "$script_dir/Cargo.toml" \
    --release \
    --locked

if [ $? -ne 0 ]; then
    echo "Build failed, go check the failure log."
    exit 1
else
    echo "Build succeeded"
fi
