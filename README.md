# Tenzir Events

This repository contains slides and supplementary materials from events where we
presented a talk.

Slides (in reverse-chronological order):

- [Suricon][suricon22] - November 2022
- [The Data Thread][datathread22] - June 2022
- [Potsdam Conference on National CyberSecurity][potsdam22] - Jun 2022
- [The International Conference on the EU Cyber Act][iceca22] - May 2022
- [Suricon][suricon21] - November 2021  
- [ZeekWeek][zeekweek21] - October 2021  
- [Suricon][suricon19] - October 2019
- [Zeek Workshop Europe][zeekshop19] - April 2019
- [DFN Conference on Security in Networked Systems][dfnconf19] - February 2019
- [BroCon][brocon18] - October 2018

## Suricon - November 2022

At [Suricon](https://suricon.net/), we showed how you can get more runway out of
your EVE JSON logs by compacting them with [VAST][vast]. We explained how
compaction works as a trigger for pipelines that aggregate the EVE logs into a
more space-efficient representation.

## The Data Thread - June 2022

At [The Data Thread](https://thedatathread.com/), we presented how 
[VAST][vast] uses [Apache Arrow](https://arrow.apache.org) as data engineering
toolkit. We showcase VAST's architecture and how Arrow helps us with
interoperability of security data.

## Potsdam Conference on National CyberSecurity - June 2022

At the [Potsdam Conference on National CyberSecurity][potsdam22-conf] we
highlighted one of the core problems of large SOCs: handling the complexity
imposed by a myriad of interconnected security tools. We showed how [VAST][vast]
can help from an architectural standpoint, as a "sidecar for the SOC."

[potsdam22-conf]: https://hpi.de/en/the-hpi/events/conferences/potsdam-conference-for-national-cybersecurity/conference.html

## The International Conference on the EU Cyber Act - May 2022

At [the International Conference on the EU Cyber Act
2022](https://eucyberact.org/), we co-presented with IBM Security's [Jason
Keirstead](https://twitter.com/BlueTeamJK) about how standardization alone is
insufficient to create an open, interoperable ecosystem of security tools. Going
back to the articles in the act, we identified market and operational themes
that need to be addressed comprehensively in order to have a real-world impact.

## Suricon - November 2021

At [Suricon 2021](https://suricon.net/suricon-2021-boston/) in Boston, we
co-presented with [DCSO](https://github.com/dcso) on a production architecture
for threat-intelligence-based detection that unifies historical and live
alerting. The architecture leverages [VAST][vast] as embedded telemetry engine
to deliver historical metadata as via [Threat
Bus](https://github.com/tenzir/threatbus), such that they appear as an `alert`
event that is indistinguishable from a live alert.

## ZeekWeek - October 2021

At [ZeekWeek 2021](https://zeek.org/zeekweek2021/), we presented how VAST can
become a Zeek logger node and transparently receive logs from a Zeek cluster in
an optimal fashion. To this end, we wrote a
[Broker](https://github.com/zeek/broker) plugin to acquire the binary log data.
We then reverse-engineered the binary message format of batched logs, which
allowed us to convert them directly into VAST's data plane using Apache Arrow.

## Suricon - October 2019

At [Suricon 2019](https://suricon.net/suricon-2019-amsterdam/) in Amsterdam, we
demonstrated how to pivot between different network telemetry with
[VAST][vast]. In particular, we showed how one can extract the PCAP packets
corresponding to a specific Suricata alert. The idea is model VAST's schema as
a graph, where edges correspond to different types and edges exist if it is
possible to join over a common record field. Users just express the pivot
destination, e.g., *"give me all PCAPs for alerts with severity N of type X"*.

## Zeek Workshop Europe - April 2019

At the [Zeek Workshop Europe](https://indico.cern.ch/event/762505/) at CERN,
we showed how to bring together [MISP](http://www.misp-project.org) and
[Zeek](https://www.zeek.org). This presentation was a joint talk with Liviu
Vâlsan who explained how to use this prototype operationally at the CERN SOC.
Our *robo investigator* expands on our approach that we presented two months
earlier (see below). In addition to correlating historical sightings, *robo*
now also interfaces with Zeek to propagate changes to intel in real time and
report "noisy" intel items.

## DFN Conference on Security in Networked Systems - February 2019

At this year's [DFN conference on Security in Networked
Systems](https://www.dfn-cert.de/veranstaltungen/sicherheitskonferenz2019.html),
we gave a demo on how to perform live correlation of threat intelligence with
historical data. Concretely, we showed how to tap into
[MISP](http://www.misp-project.org) feeds in real time and translate new
indicators into queries over old data. Our tool reports hits in historical data
back to MISP as *sightings*. This makes it possible to understand whether an
organization has been breached even before the indicator became available.

## BroCon - October 2018

At [BroCon 2018](https://www.brocon18.com) we talked about automated analysis
with [Broker](https://github.com/bro/broker). We used the example of automatic
historic intelligence lookups with [VAST][vast] to
illustrate the Broker API. Additionally, we performed a performance analysis of
Broker in terms of throughput and latency. See the [brocon18](brocon18)
directory for the complete list of accompanying material.

[suricon22]: https://github.com/tenzir/events/releases/download/suricon22/slides.pdf
[datathread22]: https://github.com/tenzir/events/releases/download/datathread22/slides.pdf
[potsdam22]: https://github.com/tenzir/events/releases/download/potsdam22/slides.pdf
[iceca22]: https://github.com/tenzir/events/releases/download/iceca22/slides.pdf
[suricon21]: https://github.com/tenzir/events/releases/download/suricon21/slides.pdf
[zeekweek21]: https://github.com/tenzir/events/releases/download/zeekweek21/slides.pdf
[suricon19]: https://github.com/tenzir/events/releases/download/suricon19/slides.pdf
[zeekshop19]: https://github.com/tenzir/events/releases/download/zeekshop19/slides.pdf
[dfnconf19]: https://github.com/tenzir/events/releases/download/dfnconf19/slides.pdf
[brocon18]: https://github.com/tenzir/events/releases/download/brocon18/slides.pdf
[vast]: https://vast.io
