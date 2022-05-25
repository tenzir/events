# Tenzir Events

This repository contains slides and examples from events that we attended.

Download slides from all events as PDF:

- [The International Conference on the EU Cyber Act 2022][iceca22]
- [Suricon 2021][suricon21]
- [Suricon 2019][suricon19]
- [Zeek Workshop Europe 2019][zeekshop19]
- [DFN Conference on Security in Networked Systems][dfnconf19]
- [BroCon 2018][brocon18]

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
alerting. The architecture leverages [VAST](https://github.com/tenzir/vast) as
embedded telemetry engine to deliver historical metadata as via [Threat
Bus](https://github.com/tenzir/threatbus), such that they appear as an `alert`
event that is indistinguishable from a live alert.

## Suricon - October 2019

At [Suricon 2019](https://suricon.net/suricon-2019-amsterdam/) in Amsterdam, we
demonstrated how to pivot between different network telemetry with
[VAST][vast]. In particular, we showed how one can extract the PCAP packets
corresponding to a specific Suricata alert. The idea is model VAST's schema as
a graph, where edges correspond to different types and edges exist if it is
possible to join over a common record field. Users just express the pivot
destination, e.g., *"give me all PCAPs for alerts with severity N of type X"*.

## Zeek Workshop Europe - Apr 2019

At the [Zeek Workshop Europe](https://indico.cern.ch/event/762505/) at CERN,
we showed how to bring together [MISP](http://www.misp-project.org) and
[Zeek](https://www.zeek.org). This presentation was a joint talk with Liviu
Vâlsan who explained how to use this prototype operationally at the CERN SOC.
Our *robo investigator* expands on our approach that we presented two months
earlier (see below). In addition to correlating historical sightings, *robo*
now also interfaces with Zeek to propagate changes to intel in real time and
report "noisy" intel items.

## DFN Conference on Security in Networked Systems - Feb 2019

At this year's [DFN conference on Security in Networked
Systems](https://www.dfn-cert.de/veranstaltungen/sicherheitskonferenz2019.html),
we gave a demo on how to perform live correlation of threat intelligence with
historical data. Concretely, we showed how to tap into
[MISP](http://www.misp-project.org) feeds in real time and translate new
indicators into queries over old data. Our tool reports hits in historical data
back to MISP as *sightings*. This makes it possible to understand whether an
organization has been breached even before the indicator became available.

## BroCon - Oct 2018

At [BroCon 2018](https://www.brocon18.com) we talked about automated analysis
with [Broker](https://github.com/bro/broker). We used the example of automatic
historic intelligence lookups with [VAST][vast] to
illustrate the Broker API. Additionally, we performed a performance analysis of
Broker in terms of throughput and latency.

Broker & Bro code:

- Python stubs: [Bro](brocon18/stub-bro), [VAST](brocon18/stub-vast)
- [Bro script](brocon18/vast.bro)
- [`bro-vast`](https://github.com/tenzir/bro-vast) package

See the [brocon18](brocon18) directory for the complete list of accompanying
material.

[iceca22]: https://github.com/tenzir/events/releases/download/iceca22/slides.pdf
[suricon21]: https://github.com/tenzir/events/releases/download/suricon21/slides.pdf
[suricon19]: https://github.com/tenzir/events/releases/download/suricon19/slides.pdf
[zeekshop19]: https://github.com/tenzir/events/releases/download/zeekshop19/slides.pdf
[dfnconf19]: https://github.com/tenzir/events/releases/download/dfnconf19/slides.pdf
[brocon18]: https://github.com/tenzir/events/releases/download/brocon18/slides.pdf
[vast]: https://github.com/tenzir/vast
