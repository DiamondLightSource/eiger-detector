FROM ghcr.io/odin-detector/odin-data-build:latest AS build

# Root of eiger-detector
COPY . /tmp/eiger-detector

# C++
WORKDIR /tmp/eiger-detector
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/odin -DODINDATA_ROOT_DIR=/odin ../cpp && \
    make -j8 VERBOSE=1 && \
    make install

COPY deploy /odin/eiger-deploy

# Python
WORKDIR /tmp/eiger-detector/python
RUN python -m pip install .

# Final image
FROM ghcr.io/odin-detector/odin-data-runtime:latest
COPY --from=build /odin /odin
ENV PATH=/odin/bin:/odin/venv/bin:$PATH
WORKDIR /odin

COPY deploy /odin/eiger-deploy

CMD ["sh", "-c", "cd /odin/eiger-deploy && zellij --layout ./startAll.kdl"]
