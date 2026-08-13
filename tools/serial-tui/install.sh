#!/bin/bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

echo "Installing Com Port Monitor"

cargo install \
    --path "${script_dir}" \
    --locked \
    --force
