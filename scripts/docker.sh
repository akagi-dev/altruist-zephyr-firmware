#!/usr/bin/env bash

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Get the project root
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

docker run --name altruist -p 5900:5900 -ti -v $PROJECT_ROOT:/workdir docker.io/zephyrprojectrtos/zephyr-build:main 

