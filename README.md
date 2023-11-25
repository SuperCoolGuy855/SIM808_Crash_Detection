# SIM808_Crash_Detection

Crash/Impact detection using ADXL345 and SIM808 to send alert

## Installation

Create a virtual enviroment using virtualenv and activate it or don't.

Installing the requirements:

```shell
pip install -r requirements.txt
```

## Usage

> Note: Notifications will only work in **localhost** or **127.0.0.1** or using **HTTPS**

### Enviroment variable

Create the following enviroment variable:

- **VAPID_PUBLIC_KEY**: used to send notifications
  
- **VAPID_PRIVATE_KEY**: used to send notifications
  
- **CRASH_HISTORY_FILE** (optional, `default="crash_history.json"`): path to json file for storing crash history
  
- **NOTIFICATION_SUBSCRIPTIONS_FILE** (optional, `default="noti_sub.json"`): path to json file for storing notification subscriptions
  
- **EMAIL** (optional, `default="example@example.com"`): email for VAPID claim
  
- **LOG_LEVEL** (optional, `default="INFO"`): set the logging level (see: [Logging Flask Documentation](https://flask.palletsprojects.com/en/3.0.x/logging/), [Logging facility for Python](https://docs.python.org/3/library/logging.html))
  
- **TEST_PASSWORD** (optional, `default="12345678"`): set the password for sending test notifications
  

Generate VAPID keys using [VAPID Key Generator](https://www.attheminute.com/vapid-key-generator)

### Starting server

#### Development mode

To start the server in development mode, run:

```shell
flask run --debug
```

Then you can access the server with [http://localhost:5000/](http://localhost:5000/)

#### Production mode

Follow the [Deploying to Production](https://flask.palletsprojects.com/en/3.0.x/deploying/) documentation to setup HTTP/WSGI server

To start the server using Gunicorn as example, run:

```shell
gunicorn -w 4 "app:app"
```

Change `-w 4` (worker number) as neccessary.

## API Endpoints

### `POST /crash`

Content type: `application/x-www-form-urlencoded`

**Form parameters:**

- `time`: Date and time of crash, format: `yyyy/MM/dd hh:mm:ss` (ex: 2023/01/23 15:32:04)
  
- `lat`: Latitude of crash location, format: `float` (ex: 10.5293...)
  
- `long`: Longtitude of crash location, format: `float` (ex: 106.123151)
  

### `POST /save_subscription`

Content type: `application/json`

> Note: Only for browser to send subscription data

### `GET /test_notification`

**Query parameters:**

- `auth`: Password for sending test notification, set in the enviroment variables