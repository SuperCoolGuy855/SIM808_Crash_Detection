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

Create the following enviroment variable using file `.env` or adding in the terminal:

- **VAPID_PUBLIC_KEY**: used to send notifications
  
- **VAPID_PRIVATE_KEY**: used to send notifications
  
- **LOG_LEVEL** (optional, `default="INFO"`): set the logging level (see: [Logging Flask Documentation](https://flask.palletsprojects.com/en/3.0.x/logging/), [Logging facility for Python](https://docs.python.org/3/library/logging.html))
  
- **TEST_PASSWORD** (optional, `default="12345678"`): set the password for sending test notifications
  

### Starting server

#### Development mode

To start the server in development mode, run:

```shell
flask run --debug
```

Then you can access the server with [http://locahost:5000]()

#### Production mode

Follow the [Deploying to Production](https://flask.palletsprojects.com/en/3.0.x/deploying/) documentation to setup HTTP/WSGI server

To start the server using Gunicorn as example, run:

```shell
gunicorn -w 4 "app:app"
```

Change `-w 4` (worker number) as neccessary.