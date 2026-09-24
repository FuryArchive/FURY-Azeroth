FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt /tmp/requirements.txt
COPY WHEELHOUSE-SHA256SUMS /tmp/WHEELHOUSE-SHA256SUMS
COPY wheels/ /tmp/wheels/

RUN cd /tmp/wheels \
    && sha256sum -c /tmp/WHEELHOUSE-SHA256SUMS \
    && pip install --no-cache-dir --no-index --find-links=/tmp/wheels -r /tmp/requirements.txt \
    && rm -rf /tmp/wheels /tmp/WHEELHOUSE-SHA256SUMS

COPY . /app/
RUN rm -rf /app/wheels

ENTRYPOINT ["python", "/app/llm_chatter_bridge.py"]
CMD ["--config", "/config/mod_llm_chatter.conf"]
