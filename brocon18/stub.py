#!/usr/bin/env python

import broker
import broker.bro
import uuid
import sys
import time

from ipaddress import *

BRO_PORT = 44444
VAST_PORT = 55555

def log(*args):
    """Prints logging output to standard error"""
    print('\033[36m>>>\033[0m', *args, file=sys.stderr)

class Endpoint:
    CONTROL_TOPIC = "/vast/control/"
    DATA_TOPIC = "/vast/data/"

    """A thin wrapper around a Broker endpoint for demo purposes."""
    def __init__(self, local_port):
        # Construct and endpoint and listen to the provided port.
        self.endpoint = broker.Endpoint()
        bound_port = self.endpoint.listen("localhost", local_port)
        log("listening at {}:{}".format("localhost", bound_port))
        # Construct a status subscriber. The boolean flag indicates that we
        # would like to receive errors as well.
        self.status_subscriber = self.endpoint.make_status_subscriber(True)
        # Create a subscriber and register the control channel.
        log("subscribing to", self.CONTROL_TOPIC)
        self.subscriber = self.endpoint.make_subscriber(self.CONTROL_TOPIC)

    def peer(self, peer_port, retry_interval=1):
        """Initiates a peering with a remote endpoint."""
        # Peer with remote side.
        self.endpoint.peer("localhost", peer_port, retry_interval)
        while True:
            x = self.status_subscriber.get()
            log(x)
            if isinstance(x, broker.Status) and x.code() == broker.SC.PeerAdded:
                break
        subscriptions = self.endpoint.peer_subscriptions()
        log("established peering successfully:", subscriptions)

class Bro(Endpoint):
    def __init__(self, local_port, peer_port):
        super().__init__(local_port)
        self.peer(peer_port)

    def query(self, expression):
        query_id = str(uuid.uuid4())
        log("subscribing to data channel for query", query_id)
        data_topic = self.DATA_TOPIC + query_id
        self.subscriber.add_topic(data_topic, True)
        event = broker.bro.Event("query", (query_id, expression))
        self.endpoint.publish(self.CONTROL_TOPIC + query_id, event)
        # Wait for query results.
        log("waiting for results")
        while True:
            topic, data = self.subscriber.get()
            result = broker.bro.Event(data)
            log("got:", topic, result.args())
            # Are we done?
            if topic == self.CONTROL_TOPIC + query_id + "/done":
                break;
        # Unpublish subscriptions for this particular query.
        log("removing subscription for", query_id)
        self.subscriber.remove_topic(data_topic, True)

    def run(self):
        self.query(":addr in 10.0.0.0/8")

class VAST(Endpoint):
    def __init__(self, local_port):
        super().__init__(local_port)

    def lookup(self, query_id, expression):
        log("answering query '{}'".format(expression))
        query_data_topic = self.DATA_TOPIC + str(query_id)
        # TODO: instead of responding with dummy data, interact with VAST to
        # get the results.
        for i in range(10):
            x = [time.ctime(),
                 [IPv4Address("10.0.0.1"),
                  IPv4Address("8.8.8.8"),
                  broker.Port(53, broker.Port.UDP),
                  broker.Port(53, broker.Port.UDP)],
                 i]
            event = broker.bro.Event("result", query_id, x)
            self.endpoint.publish(query_data_topic, event)
        log("completed query", query_id)
        event = broker.bro.Event("result", query_id, None)
        self.endpoint.publish(query_data_topic, event)

    def dispatch_control_message(self, topic, data):
        log("deconstructing Bro event:", topic, data)
        event = broker.bro.Event(data)
        (query_id, expression) = event.args()
        self.lookup(query_id, expression)

    def run(self):
        while True:
            log("waiting for commands...")
            (topic, data) = self.subscriber.get()
            self.dispatch_control_message(topic, data)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: {} <mode>".format(sys.argv[0]))
        exit(1)
    mode = sys.argv[1]
    if mode == "bro":
        bro = Bro(BRO_PORT, VAST_PORT)
        bro.run()
    elif mode == "vast":
        vast = VAST(VAST_PORT)
        vast.run()
    else:
        print("invalid mode:", mode)
        exit(1)
