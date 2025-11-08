#!/usr/bin/env bash

docker buildx build --load --platform=armhf -t sdrmodem-buster-armhf -f buster.Dockerfile .
docker buildx build --load --platform=armhf -t sdrmodem-stretch-armhf -f stretch.Dockerfile .
docker buildx build --load --platform=armhf -t sdrmodem-bullseye-armhf -f bullseye.Dockerfile .
docker buildx build --load --platform=armhf -t sdrmodem-bookworm-armhf -f bookworm.Dockerfile .
docker buildx build --load --platform=arm64 -t sdrmodem-bookworm-arm64 -f bookworm.Dockerfile .
docker buildx build --load --platform=armhf -t sdrmodem-trixie-armhf -f trixie.Dockerfile .
docker buildx build --load --platform=arm64 -t sdrmodem-trixie-arm64 -f trixie.Dockerfile .
docker buildx build --load --platform=amd64 -t sdrmodem-bionic-amd64 -f bionic.Dockerfile .
docker buildx build --load --platform=amd64 -t sdrmodem-focal-amd64 -f focal.Dockerfile .
docker buildx build --load --platform=amd64 -t sdrmodem-jammy-amd64 -f jammy.Dockerfile .
