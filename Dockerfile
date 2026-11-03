ARG ODIN_DATA_VERSION=1.13.0-beta.4
FROM ghcr.io/odin-detector/odin-data-build:${ODIN_DATA_VERSION} AS developer

FROM developer AS build

# Root of eiger-detector
COPY . /odin/eiger-detector

# C++
WORKDIR /odin/eiger-detector
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/odin -DODINDATA_ROOT_DIR=/odin ../cpp && \
    make -j8 VERBOSE=1 && \
    make install

# Python
WORKDIR /odin/eiger-detector/python
RUN python -m pip install .[sim]

FROM ghcr.io/odin-detector/odin-data-runtime:${ODIN_DATA_VERSION} AS runtime

COPY --from=build /odin /odin
COPY --from=build /venv /venv
COPY deploy /odin/eiger-deploy

RUN rm -rf /odin/eiger-detector

ENV PATH=/odin/bin:/odin/venv/bin:$PATH

WORKDIR /odin

CMD ["sh", "-c", "cd /odin/eiger-deploy && zellij --layout ./layout.kdl"]
