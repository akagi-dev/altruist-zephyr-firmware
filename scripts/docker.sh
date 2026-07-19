#!/usr/bin/env bash

PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

docker run --name altruist -p 5900:5900 -ti -v $PROJECT_ROOT:/workdir docker.io/zephyrprojectrtos/zephyr-build:main 

