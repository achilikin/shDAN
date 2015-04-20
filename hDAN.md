shDAN - small/smart house Data Acquisition Network
=========================================
shDAN stands for "small/smart house Data Acquisition Network". And "small" not necessary relates to a small house, it could be just a small network in a big house :)

Why "small"? Because of the following limitations:
* No more than 12 Data Acquisition Nodes (DAN) per subnet
* No more than 6 sensors (zones) per Data Acquisition Node
* Messages between Base Station and Data Nodes are quite small - 13 bytes only, with fixed payload's length of 4 bytes.

shDAN main components
--------------------
**ABS** - Active Base Station, replies to time sync requests from Data Nodes
**LBS** - Listening Base Station, only collects data from Data Nodes, but never transmit anything. Useful for a standalone displays or monitoring stations.
**DAN** - Data Acquisition Node
**NID** - Node ID. Base Station is always 0, DANs are in 1 to 12 range, 13-15 reserved.
**SID** - Sensor ID, up to 8 sensors per NID. Each NID always has two special SIDs: 0 is RF TX power, 7 indicates List of Sensors.
**TOS** - Type of Sensor, 1 to 15 range. For example, 1 is for Temperature, 2 Humidity and so on. For all defined types see ```dnode.h```
**EOS** - End of Session bit  
**AA** - Always Active node. Usually data nodes are in sleep mode to save battery power and wake up only to do new measurement and transmit data to a base station.

**shDAN** uses simple time-division multiplexing schema to spread different DANs' sessions in one minute. Start of DAN's transmission can be calculated as _second = (node - 1)*5_ so node 1 transmits first message at 00 sec of every minute, node 2 at 05 sec of every minute and so one.   A session cannot be longer than 5 seconds, last message should have EOS bit set to indicate End of Session, so base station can send messages to AA (Always Active) nodes or other nodes can transmit urgent data.

See SVG pictures below for details. 

shDAN topology
-------------
![hDAN topology](https://rawgithub.com/achilikin/mmr70mod/master/hDAN_topology.svg)

shDAN protocol
-------------
![hDAN diagram](https://rawgithub.com/achilikin/mmr70mod/master/hDAN_protocol.svg)
For some reason I'm getting a lot of noise on my base station receivers. So packet detection algorithm uses length of a packet as a start byte (4 bytes for now) then receives 5 bytes (4 bytes payload + 1 byte CRC) and checks for 0x55 as packet's stop byte. If 0x55 is found then CRC is calculated and checked as well. If 0x55 not found then algorithm resets its state and waits for start byte 0x04. 

shDAN messages examples
----------------------
![hDAN diagram](https://rawgithub.com/achilikin/mmr70mod/master/hDAN_messages.svg)

 