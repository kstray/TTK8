from os import stat
from google.api_core import future
from google.cloud import pubsub, iot
from google.cloud.iot_v1.types.resources import Device
import pyowm
from pyowm.utils import timestamps
import time

NAME = "ttk8-weather"
PROJECT = "wearebrews"

owm = pyowm.OWM("8da2a4a8aedc13702034b4ed7a5dbe6c")

iot_client = iot.DeviceManagerClient()

publisher = pubsub.PublisherClient()

event_topic_name = f"projects/{PROJECT}/topics/events-iot"
state_topic_name = f"projects/{PROJECT}/topics/events-iot-state"

class Weather():
        def __init__(self, lat: float, long: float) -> None:
                mgr = owm.weather_manager()
                resp = mgr.one_call(lat, long)
                self.current = resp.current
                
        def format_embedded(self) -> str:
                return f"{self.current.status};{self.current.temperature('celsius').get('temp')}"

def get_weather_for_loc(lat: float, lon: float) -> str:
        return Weather(lat, lon).format_embedded()



def set_device_config(payload: str, projectId: str, deviceRegistryLocation: str, deviceRegistryId: str, deviceId: str, **kwargs) -> Device:
        deviceName = f"projects/{projectId}/locations/{deviceRegistryLocation}/registries/{deviceRegistryId}/devices/{deviceId}"
        iot_client.modify_cloud_to_device_config(name=deviceName, binary_data=bytes(payload, encoding="utf8"))


def on_message(message):
        print(message)
        message.ack()
        data = str(message.data, encoding="utf8")

        if message.attributes["subFolder"] != "weather/location":
                return

        try:
                lat, lon = data.split(";")
                lat, lon = float(lat), float(lon)
        except Exception as e:
                # Wrong format, ignore!
                print("error", e)
                return
        
        payload = get_weather_for_loc(lat, lon)
        print("Sending", payload, "to", message.attributes["deviceId"])
        set_device_config(payload, **message.attributes)

with pubsub.SubscriberClient() as subscriber:
        name=f"projects/{PROJECT}/subscriptions/events-iot-{NAME}"
        sub = subscriber.subscribe(name, on_message)
        try:
                sub.result()
        except KeyboardInterrupt:
                sub.cancel()
