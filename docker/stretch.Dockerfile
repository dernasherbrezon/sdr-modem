FROM debian:stretch-slim
ENV DEBIAN_FRONTEND noninteractive
COPY --chmod=644 stretch.gpg /usr/share/keyrings/stretch.gpg
COPY --chmod=644 stretch-security.gpg /usr/share/keyrings/stretch-security.gpg
COPY --chmod=644 r2cloud.gpg /usr/share/keyrings/r2cloud.gpg
RUN echo 'deb [signed-by=/usr/share/keyrings/stretch.gpg] http://archive.debian.org/debian/ stretch main non-free contrib' > /etc/apt/sources.list
RUN echo 'deb [signed-by=/usr/share/keyrings/stretch-security.gpg] http://archive.debian.org/debian-security/ stretch/updates main non-free contrib' >> /etc/apt/sources.list
RUN echo "deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://r2cloud.s3.amazonaws.com stretch main" >> /etc/apt/sources.list
RUN echo "deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://r2cloud.s3.amazonaws.com/cpu-generic stretch main" >> /etc/apt/sources.list
RUN apt-get update && apt-get install --no-install-recommends -y build-essential file valgrind cmake libconfig-dev pkg-config libvolk2-dev libprotobuf-c-dev libvolk2-bin libiio-dev check librtlsdr-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*