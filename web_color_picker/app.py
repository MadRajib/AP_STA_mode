from flask import Flask
from flask import render_template

app = Flask(__name__)

@app.route("/globe")
def home():
    return render_template("index.html")
