#!/usr/bin/env python

import broker
import broker.bro
import uuid
import sys
import time

from ipaddress import *

# -- constants ----------------------------------------------------------------

PORT = 43000
CONTROL_TOPIC = "/vast/control/"
DATA_TOPIC = "/vast/data/"

# -- utilities ----------------------------------------------------------------

def log(*args):
    """Prints logging output to standard error"""
    print('\033[36m>>>\033[0m', *args, file=sys.stderr)

# -- Bro ----------------------------------------------------------------------

class Bro:
    def __init__(self, port):
        self.endpoint = broker.Endpoint()
        # Create a subscriber and register control and data channel.
        topics = [CONTROL_TOPIC, DATA_TOPIC]
        self.subscriber = self.endpoint.make_subscriber(topics)
        # Construct a status subscriber. The boolean flag indicates that we
        # would like to receive errors as well.
        self.status_subscriber = self.endpoint.make_status_subscriber(True)
        log("peering with VAST")
        self.endpoint.peer("localhost", port, 1.0)
        while True:
            x = self.status_subscriber.get()
            if isinstance(x, broker.Status) and x.code() == broker.SC.PeerAdded:
                break
        subscriptions = self.endpoint.peer_subscriptions()
        log("established peering successfully")

    def query(self, expression):
        query_id = str(uuid.uuid4())
        event = broker.bro.Event("query", query_id, expression)
        self.endpoint.publish(CONTROL_TOPIC, event)
        log("performing lookup for", expression)
        while True:
            topic, data = self.subscriber.get()
            event = broker.bro.Event(data)
            log(topic, event.args())
            (qid, result) = event.args()
            if result is None:
                break;

    def run(self):
        self.query(":addr in 10.0.0.0/8")

# -- VAST ---------------------------------------------------------------------

class VAST:
    def __init__(self, port):
        self.endpoint = broker.Endpoint()
        # Create a subscriber and register control and data channel.
        topics = [CONTROL_TOPIC, DATA_TOPIC]
        self.subscriber = self.endpoint.make_subscriber(topics)
        self.endpoint.listen("localhost", port)

    def lookup(self, query_id, expression):
        log("answering query '{}'".format(expression))
        query_data_topic = DATA_TOPIC
        results = self.answer(expression)
        for x in results:
            event = broker.bro.Event("result", query_id, x)
            self.endpoint.publish(query_data_topic, event)
        # A null/none value signals that the query has completed.
        event = broker.bro.Event("result", query_id, None)
        self.endpoint.publish(query_data_topic, event)

    def answer(self, expression):
        # We just generate dummy data here.
        f = lambda x: [x, time.ctime(), [IPv4Address("10.0.0.1")]]
        return map(f, range(10))

    def run(self):
        while True:
            log("waiting for commands")
            (topic, data) = self.subscriber.get()
            event = broker.bro.Event(data)
            (query_id, expression) = event.args()
            self.lookup(query_id, expression)

# -- main ---------------------------------------------------------------------

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: {} <mode> [port]".format(sys.argv[0]))
        exit(1)
    mode = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else PORT
    if mode == "bro":
        bro = Bro(port)
        bro.run()
    elif mode == "vast":
        vast = VAST(port)
        vast.run()
    else:
        print("invalid mode:", mode)
        exit(1)
