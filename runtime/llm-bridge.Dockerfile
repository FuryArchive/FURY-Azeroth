FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt /tmp/requirements.txt
COPY wheels/ /tmp/wheels/

RUN pip install --no-cache-dir --no-index --find-links=/tmp/wheels -r /tmp/requirements.txt \
    && rm -rf /tmp/wheels

COPY . /app/
RUN rm -rf /app/wheels

ENTRYPOINT ["python", "/app/llm_chatter_bridge.py"]
CMD ["--config", "/config/mod_llm_chatter.conf"]
