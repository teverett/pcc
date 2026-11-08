#!/usr/bin/env bash
set -eu

: "${CONDA_PREFIX:?CONDA_PREFIX must be set}"
: "${GITHUB_ENV:?GITHUB_ENV must be set}"
printf 'LD_LIBRARY_PATH=%s\n' "$CONDA_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" >> "$GITHUB_ENV"
