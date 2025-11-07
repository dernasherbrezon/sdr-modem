FROM debian:trixie-slim
ENV DEBIAN_FRONTEND noninteractive
COPY --chmod=644 r2cloud.gpg /usr/share/keyrings/r2cloud.gpg
RUN echo "deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://r2cloud.s3.amazonaws.com trixie main" >> /etc/apt/sources.list
RUN echo "deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://r2cloud.s3.amazonaws.com/cpu-generic trixie main" >> /etc/apt/sources.list
RUN apt-get update && apt-get install --no-install-recommends -y build-essential file valgrind cmake libconfig-dev pkg-config libvolk2-dev libprotobuf-c-dev libvolk2-bin libiio-dev check librtlsdr-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*