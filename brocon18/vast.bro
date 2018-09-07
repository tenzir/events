##! A script that speaks to VAST via Broker.

# Keep Bro alive even though there are no packets.
redef exit_only_after_terminate = T;

module VAST;

export {
	## The hostname or address where VAST runs.
	const HOST = "localhost" &redef;

	## The port where VAST listens.
	const PORT = 55555/tcp &redef;

	## The Broker topic for the control channel.
	const CONTROL_TOPIC = "/vast/control/";

	## The Broker topic for the data channel.
	const DATA_TOPIC = "/vast/data/";
}

## The event that this script sends to VAST to create a new query.
global query: event(id: string, expression: string);

## The event that VAST sends back.
event result(uuid: string, data: Broker::Data)
	{
	#if (|data| == 0)
	#	{
	#	print fmt("query %s completed", uuid);
	#	Broker::unsubscribe(DATA_TOPIC + uuid); 
	#	return;
	#	}
	print data;
	}

function generate_uuid() : string
	{
	# TODO: figure out how to generate a random 16-byte string.
	#local bytes = ....;
	#return uuid_to_string(bytes);
	return "6ef0cb1a-f0b2-44d7-9303-6000091e35e3";
	}

## Performs a lookup of an expression in VAST. Results arrive asynchronously
## via the ``result`` event.
## 
## expresion: The query expression.
##
## Returns: The UUID of the query.
function lookup(expression: string): string
	{
	local query_id = generate_uuid();
	# Subscribe to results and then submit the query.
	Broker::subscribe(DATA_TOPIC + query_id);
	local e = Broker::make_event(query, query_id, expression);
	Broker::publish(CONTROL_TOPIC, e);
	return query_id;
	}

event Broker::peer_added(endpoint: Broker::EndpointInfo, msg: string)
	{
	lookup(":addr in 10.0.0.0/8");
	}

event Broker::peer_lost(endpoint: Broker::EndpointInfo, msg: string)
	{
	terminate();
	}

event bro_init() 
	{
	Broker::subscribe(CONTROL_TOPIC);
	Broker::peer(HOST, PORT);
	}
