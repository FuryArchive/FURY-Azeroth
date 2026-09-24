FROM python:3.12-slim

WORKDIR /app

COPY llm-chatter/requirements.txt /tmp/requirements.txt
COPY llm-chatter/wheels/ /tmp/wheels/

RUN pip install --no-cache-dir --no-index --find-links=/tmp/wheels -r /tmp/requirements.txt \
    && rm -rf /tmp/wheels

COPY llm-chatter/ /app/
RUN rm -rf /app/wheels

ENTRYPOINT ["python", "/app/llm_chatter_bridge.py"]
CMD ["--config", "/config/mod_llm_chatter.conf"]
