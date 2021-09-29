# IOTclock
I can't get up and get late to my job every morning, i am very afraid of lossing my job, so i whish this project giving me help. 
  
Notice:  
1.both client and server are implement form rasberry pi.  
2.client is added a pull-down resistor button and detect input from gpio.  
3.server is added a passive Buzzer and trigger it from gpio.  
4.the bassic frame of server communication is implement from eloop as hostapd, copyright Jouni Malinen  
  
How to use:  
1.for get up:
complier iotser tnen run "iotser -c 10:00", and the buzzer will work on time, it will non stop untill someone press the button on the remote client placed in my company. remote client should run "iotclt server_adress"  

2.for remote alarm:  
server run "iotser -a", buzzer work if remoteclient pressing button 1 time in 1 sec, and stop for pressing 2 times in 1 sec.(you should use above method to stop get up buzzer)  

3.for really want to sleep:  
unpluging the server power is the best way.
