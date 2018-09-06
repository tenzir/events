#!/usr/bin/env python

import broker
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
    def __init__(self, local_port, peer_port):
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
        # Peer with remote side.
        self.peer(peer_port)

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
        super().__init__(local_port, peer_port)

    def query(self, expression):
        query_id = str(uuid.uuid4())
        control_topic = self.CONTROL_TOPIC + query_id
        data_topic = self.DATA_TOPIC + query_id
        # Create a subscriber and publish the query.
        log("subscribing to control and data channel for query", query_id)
        self.subscriber.add_topic(control_topic, True)
        self.subscriber.add_topic(data_topic, True)
        log("publishing query to", control_topic)
        self.endpoint.publish(control_topic, expression)
        # Wait for query results.
        log("waiting for results")
        while True:
            topic, data = self.subscriber.get()
            log("got:", topic, data)
            # Are we done?
            if topic == self.CONTROL_TOPIC + query_id + "/done":
                break;
        # Unpublish subscriptions for this particular query.
        log("removing subscription for", query_id)
        self.subscriber.remove_topic(control_topic, True)
        self.subscriber.remove_topic(data_topic, True)

    def run(self):
        self.query(":addr in 10.0.0.0/8")

class VAST(Endpoint):
    def __init__(self, local_port, peer_port):
        super().__init__(local_port, peer_port)

    def dispatch_control_message(self, topic, data):
        log("dispatching", topic, data)
        xs = topic.split(self.CONTROL_TOPIC)
        assert len(xs) > 1
        query_id = uuid.UUID(xs[1])
        self.answer_query(query_id, data)

    def answer_query(self, query_id, data):
        query_data_topic = self.DATA_TOPIC + str(query_id)
        log("publishing query results to", query_data_topic)
        for i in range(10):
            x = broker.Data([time.ctime(),
                             [IPv4Address("10.0.0.1"),
                              IPv4Address("8.8.8.8"),
                              broker.Port(53, broker.Port.UDP),
                              broker.Port(53, broker.Port.UDP)],
                             i])
            self.endpoint.publish(query_data_topic, x)
        log("completed query", query_id)
        query_done_topic = self.CONTROL_TOPIC + str(query_id) + "/done"
        self.endpoint.publish(query_done_topic, x)

    def run(self):
        while True:
            log("waiting for next message")
            (topic, data) = self.subscriber.get()
            log("got new message:", topic, data)
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
        vast = VAST(VAST_PORT, BRO_PORT)
        vast.run()
    else:
        print("invalid mode:", mode)
        exit(1)
