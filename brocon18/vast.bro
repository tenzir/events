##! A script that speaks to VAST via Broker.

# Keep Bro alive even though there are no packets.
redef exit_only_after_terminate = T;

module VAST;

export {
  ## The hostname or address where VAST runs.
  const HOST = "localhost" &redef;

  ## The port where VAST listens.
  const PORT = 43000/tcp &redef;

  ## The Broker topic for the control channel.
  const CONTROL_TOPIC = "/vast/control";

  ## The Broker topic for the data channel.
  const DATA_TOPIC = "/vast/data";
}

## The event that this script sends to VAST to create a new query.
global query: event(id: string, expression: string);

## The event that VAST sends back in response to a query.
global result: event(uuid: string, data: any);

event result(uuid: string, data: any)
  {
  # A valid result is a vector over data. A null value signifies that the query
  # has terminated.
  switch (data)
    {
    default:
      terminate();
      break;
    case type vector of any as xs:
      print xs;
      break;
    }
  }

## Generates a random 16-byte UUID.
##
## Returns: A random UUID, e.g., ``6ef0cb1a-f0b2-44d7-9303-6000091e35e3``.
function random_uuid() : string
  {
  # We use the 11 bytes of unique_id() with a fixed 5-byte prefix to end up
  # with 16 bytes for the UUID.
  local uid = unique_id("VAST-");
  # unique_id() doesn't always return 11 bytes! In this case the result needs
  # to padded/trimmed.
  if ( |uid| < 16 )
    while ( |uid| < 16 )
      uid = cat(uid, "-");
  else if ( |uid| > 16 )
    uid = sub_bytes(uid, 0, 16);
  return uuid_to_string(uid);
  }

## Performs a lookup of an expression in VAST. Results arrive asynchronously
## via the ``result`` event.
##
## expresion: The query expression.
##
## Returns: The UUID of the query.
function lookup(expression: string): string
  {
  local query_id = random_uuid();
  local e = Broker::make_event(query, query_id, expression);
  Broker::publish(CONTROL_TOPIC, e);
  return query_id;
  }

event Broker::peer_added(endpoint: Broker::EndpointInfo, msg: string)
  {
  print "established peering successfully";
  lookup(":addr in 192.168.1.0/24");
  }

event Broker::peer_lost(endpoint: Broker::EndpointInfo, msg: string)
  {
  terminate();
  }

event bro_init()
  {
  Broker::subscribe(CONTROL_TOPIC);
  Broker::subscribe(DATA_TOPIC);
  Broker::peer(HOST, PORT);
  }
