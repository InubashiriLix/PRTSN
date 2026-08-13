#!/bin/bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

echo "Installing BLE Agent Client"

cargo install \
    --path "${script_dir}" \
    --locked \
    --force
