FROM python:3.10-slim-bullseye

WORKDIR /python-docker

COPY requirements.txt requirements.txt
RUN pip3 install -r requirements.txt

COPY app.py	app.py
COPY static static
COPY templates templates

CMD [ "python3", "-m" , "flask", "run", "--host=0.0.0.0"]
