from flask import Flask, render_template, request, jsonify
from dotenv import load_dotenv
from pywebpush import webpush, WebPushException
from werkzeug import exceptions
from werkzeug.middleware.proxy_fix import ProxyFix
import os
import json
import datetime

import pprint

class LoggingMiddleware(object):
    def __init__(self, app):
        self._app = app

    def __call__(self, env, resp):
        errorlog = env['wsgi.errors']
        pprint.pprint(('REQUEST', env), stream=errorlog)

        def log_response(status, headers, *args):
            pprint.pprint(('RESPONSE', status, headers), stream=errorlog)
            return resp(status, headers, *args)

        return self._app(env, log_response)

def get_crash_history():
    if not os.path.exists(CRASH_HISTORY_FILE):
        with open(CRASH_HISTORY_FILE, "w") as f:
            json.dump([], f)
    with open(CRASH_HISTORY_FILE, "r") as f:
        return json.load(f)


def get_notification_subscriptions():
    if not os.path.exists(NOTIFICATION_SUBSCRIPTIONS_FILE):
        with open(NOTIFICATION_SUBSCRIPTIONS_FILE, "w") as f:
            json.dump([], f)
    with open(NOTIFICATION_SUBSCRIPTIONS_FILE, "r") as f:
        return json.load(f)


def json_list_append(filename, value):
    if not os.path.exists(filename):
        with open(filename, "w") as f:
            json.dump([], f)
    with open(filename, "r") as f:
        data = json.load(f)
    data.append(value)
    with open(filename, "w") as f:
        json.dump(data, f)


def degree_convert(num, lat=True):
    if lat:
        degree = num[:2]
        minute = num[2:]
    else:
        degree = num[:3]
        minute = num[3:]

    return f"{degree}.{minute/60}"


def time_format(time):
    return datetime.datetime.strptime(
        time + " +0000", "%Y/%m/%d %H:%M:%S %z"
    ).timestamp()


def send_noti(message, title="Crash detected"):
    app.logger.info("Sending notification")

    sub_list = get_notification_subscriptions()
    for sub in sub_list:
        app.logger.debug(f"Sending notification to {sub['endpoint']}")
        try:
            webpush(
                subscription_info=sub,
                data=json.dumps({"title": title, "message": message}),
                vapid_private_key=os.getenv("VAPID_PRIVATE_KEY"),
                vapid_claims={
                    "sub": f"mailto:{os.getenv('EMAIL', 'example@example.com')}"
                },
            )
        except WebPushException as ex:
            if ex.response.status_code in [410, 404]:
                app.logger.info(f"Subscription {sub['endpoint']} expired or not found")
                sub_list.remove(sub)
                with open(NOTIFICATION_SUBSCRIPTIONS_FILE, "w") as f:
                    json.dump(sub_list, f)
                continue
            app.logger.error(f"Error sending notification: {ex}")
            # Mozilla returns additional information in the body of the response.
            try:
                extra = ex.response.json()
                app.logger.error(
                    "Remote service replied with a {}:{}, {}".format(
                        extra["code"], extra["errno"], extra["message"]
                    )
                )
            except ValueError:
                # No json in response
                pass


load_dotenv()
CRASH_HISTORY_FILE = os.getenv("CRASH_HISTORY_FILE", "crash_history.json")
NOTIFICATION_SUBSCRIPTIONS_FILE = os.getenv(
    "NOTIFICATION_SUBSCRIPTIONS_FILE", "noti_sub.json"
)

if not os.getenv("VAPID_PRIVATE_KEY"):
    raise Exception("VAPID_PRIVATE_KEY not set")
if not os.getenv("VAPID_PUBLIC_KEY"):
    raise Exception("VAPID_PUBLIC_KEY not set")

app = Flask(__name__)

# app.wsgi_app = LoggingMiddleware(app.wsgi_app)
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1, x_proto=1, x_host=1, x_prefix=1)

app.logger.setLevel(os.getenv("LOG_LEVEL", "INFO"))


@app.route("/")
def index():
    app.logger.debug(f"Client: {request.remote_addr}")
    return render_template(
        "main.html",
        history=sorted(get_crash_history(), key=lambda x: x["time"], reverse=True),
        public_key=os.getenv("VAPID_PUBLIC_KEY"),
    )


@app.post("/crash")
def crash():
    app.logger.info("Crash detected")

    app.logger.debug(f"Data: {request.get_data()}")

    try:
        time = request.form["time"]
        lat = request.form["lat"]
        lng = request.form["long"]
    except KeyError:
        app.logger.error("Malformed request")
        return jsonify({"message": "Malformed request"}), 400

    app.logger.debug(f"Before parsing: time: {time}, lat: {lat}, long: {lng}")

    try:
        time = time_format(time)
    except ValueError:
        app.logger.error("Invalid time format")
        return jsonify({"message": "Invalid time format"}), 400

    try:
        lat = float(lat)
        lng = float(lng)
    except ValueError:
        app.logger.error("Invalid lat or long")
        return jsonify({"message": "Invalid lat or long"}), 400

    crash_data = {
        "time": time,
        "lat": lat,
        "lng": lng,
    }
    app.logger.debug(crash_data)
    crash_history_list = get_crash_history()
    if not any(d.get("time") == time for d in crash_history_list):
        json_list_append(CRASH_HISTORY_FILE, crash_data)

    send_noti(f"Crash at: {lat}, {lng}")

    return jsonify({"data": crash_data}), 200


@app.post("/save_subscription")
def save_sub():
    app.logger.info("New subscription received")

    try:
        data = request.get_json(force=True)
    except Exception:
        app.logger.error("Invalid subscription data")
        return jsonify({"message": "Invalid request"}), 400

    app.logger.info(f"Subscription endpoint: {data['endpoint']}")
    app.logger.debug(f"Subscription data: {data}")

    sub_list = get_notification_subscriptions()
    if not data in sub_list:
        app.logger.info("Saving new subscription")

        json_list_append(NOTIFICATION_SUBSCRIPTIONS_FILE, data)
    return jsonify({"data": data}), 200


@app.get("/test_notification")
def test_noti():
    app.logger.info("Receive request to test notification")

    auth = request.args.get("auth", "")
    if auth != os.getenv("TEST_PASSWORD", "12345678"):
        app.logger.warn("Unauthorized test request")
        raise exceptions.NotFound

    send_noti("Test notification")
    return "OK", 200
