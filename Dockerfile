FROM ghcr.io/odin-detector/odin-data-build:1.12.0 AS developer

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

FROM ghcr.io/odin-detector/odin-data-runtime:1.12.0 AS runtime

COPY --from=build /odin /odin
COPY --from=build /venv /venv
COPY deploy /odin/eiger-deploy

RUN rm -rf /odin/eiger-detector

ENV PATH=/odin/bin:/odin/venv/bin:$PATH

WORKDIR /odin

CMD ["sh", "-c", "cd /odin/eiger-deploy && zellij --layout ./layout.kdl"]
