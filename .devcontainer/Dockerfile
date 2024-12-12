FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="3.31.1"
COPY ./reinstall-cmake.sh /tmp/
RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# Install development tools
RUN apt-get update \
  && export DEBIAN_FRONTEND=noninteractive \
  && apt-get -y install --no-install-recommends \
    clang-tidy \
    clang-format \
    clangd \
    cppcheck \
    doxygen \
    graphviz \
    codespell \
    ninja-build \
    lcov \
    python3-pip \
    python3-venv \
    python3-jinja2 \
    python3-pygments \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# Adjust installed vcpkg
RUN git config --system --add safe.directory "${VCPKG_ROOT}" \
  && cd "${VCPKG_ROOT}" \
  && git fetch --unshallow

# [Optional] Uncomment this section to install additional vcpkg ports.
# RUN su vscode -c "${VCPKG_ROOT}/vcpkg install <your-port-name-here>"

# [Optional] Uncomment this section to install additional packages.
# RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
#     && apt-get -y install --no-install-recommends <your-package-list-here>
