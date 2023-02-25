# pubsub-cpp-example

How to test

===

0. Run single or multiple pubsub servers.
> pubsubserver [listen port]

1. Run a pubsub gateway.
> pubsubgateway [gateway port] [server port1] [server port2] ...

2. Connect to the pubsub gateway and send commands using telnet.
> telnet localhost [gateway port]

Subscribe a user to the topic.
<code>
user sub topic
</code>

Publish a message to the topic.
<code>
user pub topic message
</code>
