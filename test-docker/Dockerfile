ARG UBUNTU_VERSION=18.04 //Default value provided

#FROM docker.epiq.rocks/epiq-sw-dev-env-${UBUNTU_VERSION}
FROM ubuntu:${UBUNTU_VERSION}

# Update the package list, install sudo, create a non-root user, and grant password-less sudo permissions
RUN apt update && apt install -y apt-utils build-essential kmod gcc
RUN apt install -y sudo vim 
RUN adduser --disabled-password --gecos "" sidekiq 
RUN adduser sidekiq sudo
RUN echo 'sidekiq ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Set the non-root user as the default user
USER sidekiq

# Set the working directory
WORKDIR /home/sidekiq/


ENTRYPOINT ["tail", "-f", "/dev/null"]
