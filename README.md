# Lobaro-CoAP

[![Build Status](https://travis-ci.org/Lobaro/lobaro-coap.svg?branch=master)](https://travis-ci.org/Lobaro/lobaro-coap)
[![Gitter Chat](http://img.shields.io/badge/chat-online-brightgreen.svg)](https://gitter.im/lobaro-iot/Lobby)

Complete CoAP Implementation in C.
Despite designed for embedded systems (e.g. ARM Cortex-M0/M3, AVR, ESP8266) it can be used on nearly every system that has c-lang support.

* Royalty-free CoAP stack
* complete request/response logic
* unified Client & Server
* easy to use
* small memory footprint
* using C99 stdlib, suitable for embedded projects
* detached packet receive/send logic
* Arduino support (experimental)

Follow [Lobaro on Twitter](https://twitter.com/LobaroHH) to get latest news about iot projects using our CoAP implementation!

# Demo/Example
* ESP8266, cheap WIFI Soc:
[Example Eclipse Project](https://github.com/Lobaro/lobaro-coap-on-esp8266)

[Step by Step Tutorial](http://www.lobaro.com/lobaro-coap-on-esp8266/)

... more to come soon!

# Related
* GoLang CoAP Client implementation & CGO wrapper for this C lib: [Lobaro CoAP-go](https://github.com/Lobaro/coap-go)

# Future development
We use the stack internally at lobaro to build universal gateway / sensor systems for our customers. Additions will be constantly merged into this repository. Also we plan a big redesign until the end of 2017. With this update (which is ongoing already) we include many learnings from our other project. 

# Contribute
We appreciate any feedback, do not hesitate to create issues or pull requests.

