import argparse

import logging
from logging.handlers import TimedRotatingFileHandler

from http.server import BaseHTTPRequestHandler, HTTPServer

from pythonosc import dispatcher
from pythonosc import osc_server

import re
import uuid
import json
import time
import os
import sys

from websocket import create_connection
import threading

# Globals
logger = logging.getLogger('obs_record')

current_cue = ""
start_cues = ["0.0.9", "9.80.93", "9.91.0"]
end_cues = ["0.0.1", "9.80.93", "9.91.0"]


intended_recording_status: bool = False
recording_active: bool = False
obs_active: bool = False
service_active: bool = True

obs_ip: str = "192.168.31.111:4445"

disguise_host_ip: str = "192.168.101.112"
disguise_port: int = 7400

webpage_host_ip: str = "localhost"
webpage_port: int = 8081

heartbeat_time = time.time()

lock = threading.RLock()


def print_handler(address, *_args):
    logger.info(f"{address}: {_args}")


def default_handler(address, *_args):
    logger.info(f"DEFAULT {address}: {_args}")


def blank_handler(address, *_args):
    pass


def cue_handler(address, *_args):
    global current_cue, intended_recording_status
    returned_string = _args[0]

    logger.debug('ReturnedString: ' + returned_string)

    new_cue = get_cue(returned_string)

    if new_cue == -1:
        return

    if new_cue == current_cue:
        return

    logger.info("Cue changed: " + str(current_cue) + " -> " + str(new_cue))

    current_cue = new_cue

    if current_cue in end_cues and current_cue in start_cues:
        intended_recording_status = True
        try:
            logger.info("Stopping then starting recording")
            obs_send(obs_ip, ["StopRecord", "StartRecord"])
        except TypeError:
            pass

    if current_cue in end_cues:
        intended_recording_status = False
        try:
            logger.info("Stopping recording")
            obs_send(obs_ip, "StopRecord")
        except TypeError:
            pass

    if current_cue in start_cues:
        intended_recording_status = True
        try:
            logger.info("Starting recording")
            obs_send(obs_ip, "StartRecord")
        except TypeError:
            pass


def uuid_string():
    return str(uuid.uuid4())


def check_recording_state():
    global intended_recording_status
    recording_active = obs_send(obs_ip, "GetRecordStatus", "outputActive")[1]

    success = True
    
    if not recording_active and intended_recording_status == True:
        logger.warning("Recording had failed to start on cue")
        count = 0
        while(obs_send(obs_ip, ["StartRecord", "GetRecordStatus"], "outputActive")[1] == False):
            
            logger.error("Still unable to start recording")
            count += 1
            time.sleep(0.1)
            if count > 5:
                return recording_active, False
        

    if recording_active and intended_recording_status == False:
        logger.warning("Recording had failed to stop on cue")
        count = 0
        while(obs_send(obs_ip, ["StopRecord", "GetRecordStatus"], "outputActive")[1] == True):
            count += 1
            time.sleep(0.1)
            if count > 5:
                logger.error("Still unable to stop recording")
                return recording_active, False
    
    logger.info("Recording status correct")


    return recording_active, True


def search_cue_string(cue_string: str, search_string: str):
    # Search for a particular regex cue pattern within the main string
    try:
        m = re.search(search_string, cue_string)
        found_cue = m.group(0)
        return found_cue
    except (ValueError, IndexError, AttributeError):
        return -1


def get_cue(raw_cue_string):
    if '|' not in raw_cue_string:
        return -1

    split_cue_string = raw_cue_string.split('|')

    if len(split_cue_string) <= 0:
        return -1

    cue_string = split_cue_string[0]
    logger.debug(cue_string)

    # First check for timecode xx:xx:xx:xx
    decimal_string = search_cue_string(cue_string, r'\d\d:\d\d:\d\d:\d\d')
    # We don't handle timecode strings, so return -1
    if decimal_string != -1:
        return -1

    # if not then check for a decimal cue xx.xx.xx
    decimal_string = search_cue_string(cue_string, r'\d+\.\d+\.\d+')
    if decimal_string != -1:
        return decimal_string

    # then check for integer cue x
    decimal_string = search_cue_string(cue_string, r'\d+')
    if decimal_string != -1:
        return -1

    logger.warning("This is bad, the cue type is neither decimal, timecode, or integer")
    return -1


def get_rpc(json_data):
    if "d" in json_data:
        if "negotiatedRpcVersion" in json_data["d"]:
            return int(json_data["d"]["negotiatedRpcVersion"])
    return -1


def check_success(json_data):
    if "d" in json_data:
        if "requestStatus" in json_data["d"]:
            if "result" in json_data["d"]["requestStatus"]:
                return bool(json_data["d"]["requestStatus"]["result"])
    return False


def get_response(json_data, check_string):
    if "d" in json_data:
        if "responseData" in json_data["d"]:
            if check_string in json_data["d"]["responseData"]:
                return bool(json_data["d"]["responseData"][check_string])
    return None


def obs_send_and_check(ws, command):
    ws.send("{\"op\": 6,\"d\": {\"requestType\": \"" + command + "\",\"requestId\": \"" + uuid_string() + "\"}}")
    result = ws.recv()
    success = check_success(json.loads(result))
    logger.debug("OBS Request Success: '%s'" % success)

    return result, success


def obs_send(ip_string, commands, check_string=""):
    global obs_active
    logger.debug("Sending " + str(commands) + " to OBS")
    try:
        ws = create_connection("ws://" + ip_string + "/", timeout=2)
        with lock:
            obs_active = True
        logger.debug("Connection to OBS established")
    except:
        logger.warning("OBS Websocket Timeout")
        with lock:
            obs_active = False
        return False, None
    logger.debug(ws.recv())

    ws.send("{\"op\": 1,\"d\":{\"rpcVersion\":1}}")
    result = ws.recv()
    rpc = get_rpc(json.loads(result))
    if rpc == -1:
        logger.warning("Unable to negotiate rpc version")
        return False, None
    logger.debug("Negotiated rpc version: " + str(rpc))

    response = None
    result = None
    success = None

    if isinstance(commands, list):
        for command in commands:
            result, success = obs_send_and_check(ws, command)
    else:
        result, success = obs_send_and_check(ws, commands)
        logger.debug(result)

    if check_string != "":
        response = get_response(json.loads(result), check_string)

    ws.close()
    return success, response


def get_json():
    with lock:
        json_state = {
            "service_active": service_active,
            "obs_active": obs_active,
            "recording_active": recording_active,
            "start_trigger": start_cues,
            "end_trigger": end_cues,
            "current_cue": current_cue,
            "time_since_heartbeat": int(time.time() - heartbeat_time)
        }

        return json.dumps(json_state)



class JSONServer(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != '/obs_record/json':
            return

        self.send_response(200)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.wfile.write(bytes(get_json(), "utf-8"))


def start_web_server(_web_server):
    _web_server.serve_forever()


def heartbeat():
    global lock
    global recording_active, heartbeat_time

    recording_active, success = check_recording_state()
    if recording_active is None:
        recording_active = False

    with lock:
        heartbeat_time = time.time()
    logger.info("Heartbeat")
    threading.Timer(30, heartbeat).start()


if __name__ == "__main__":
    log_file_handler = TimedRotatingFileHandler(filename='runtime.log', when='D', interval=1, backupCount=10,
                                        encoding='utf-8',
                                        delay=False)

    log_console_handler = logging.StreamHandler(sys.stdout)

    log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')

    log_file_handler.setFormatter(log_formatter)
    log_console_handler.setFormatter(log_formatter)


    logger.setLevel(logging.INFO)

    logger.addHandler(log_file_handler)
    logger.addHandler(log_console_handler)

    parser = argparse.ArgumentParser()
    parser.add_argument("--ip",
                        default=disguise_host_ip, help="The ip to listen on")
    parser.add_argument("--port",
                        type=int, default=disguise_port, help="The port to listen on")
    args = parser.parse_args()

    dispatcher = dispatcher.Dispatcher()
    dispatcher.map("/d3/showcontrol/sectionhint", cue_handler)
    dispatcher.set_default_handler(blank_handler)

    server = osc_server.ThreadingOSCUDPServer((args.ip, args.port), dispatcher)

    webServer = HTTPServer((webpage_host_ip, webpage_port), JSONServer)
    logger.info("Server started http://%s:%s" % (webpage_host_ip, webpage_port))

    try:
        logger.info("Initiating heartbeat")
        heartbeat()

        logger.info("Serving website")
        t = threading.Thread(target=start_web_server, args=(webServer,)).start()

        logger.info("Serve OSC on {}".format(server.server_address))
        server.serve_forever()

    except KeyboardInterrupt:
        print("")
        os._exit(1)
