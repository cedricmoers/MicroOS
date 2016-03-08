#include "communicator.h"

Communicator::Communicator(Robot* robot) :
	_transmission_msg(0),
	_update_messages{MAVLINK_MSG_ID_GPIO},
	_robot(robot)
{
	_channels[0] = new Channel(&COMM_PORTA, new ProtocolMavlink);
	
	_transmission_time = micros();
	_heartbeat_time = micros();
}

int Communicator::start()
{
	COMM_PORTA.begin(COMM_PORTA_BAUD);
	_channels[0]->start();
	
	return 0;
}

int Communicator::update()
{
	unsigned long time = micros();
	
	if(time>_transmission_time){
		_transmission_time += TRANS_WAKEUP;
		transmit(_update_messages[_transmission_msg]);
		_transmission_msg++;
		if(_transmission_msg>=NUM_UPDATE_MESSAGES){ _transmission_msg = 0; };
	}
	if(time>_receive_time){
		_receive_time += RECV_WAKEUP;
		receive();
	}
	if(time>_heartbeat_time){
		_heartbeat_time += HEARTBEAT_WAKEUP;
		transmit(MAVLINK_MSG_ID_HEARTBEAT);	
	}
	
	return 0;
}

int Communicator::receive()
{
	for(uint8_t k=0; k<NUM_COMM_PORTS ; k++){
		while(_channels[k]->receive()){
			action(*reinterpret_cast<const mavlink_message_t*>(_channels[k]->getMessage()));
		}
	}

	return 0;
}

int Communicator::transmit(uint8_t msgID)
{
	bool send = true;
	mavlink_message_t msg;

	// try to send the requested message
	// if the message is not found, we cancel the send request
    switch(msgID){
	    case MAVLINK_MSG_ID_HEARTBEAT:
		    mavlink_msg_heartbeat_pack( _robot->ID(), _robot->ID(), &msg, 0, millis(), 0, 0, 0);
		    break;
		case MAVLINK_MSG_ID_GPIO:
			mavlink_msg_gpio_pack(_robot->ID(), _robot->ID(), &msg,  millis(), 0, MicroOS::getGPoutFloat(), MicroOS::getGPoutInt());
			break;	
	    default:
	    send = false;
	    break;
	}
	
	/*actual sending*/
	if(send){
		_channels[0]->send(&msg);
	}
	
	return (int)send;
}

void Communicator::sendThreadInfo(uint8_t ID, char* name, uint8_t priority, uint32_t duration, uint32_t latency, uint32_t total_duration, uint32_t total_latency, uint32_t number_of_executions)
{
	mavlink_message_t msg;
	mavlink_msg_thread_info_pack(_robot->ID(), _robot->ID(), &msg, millis(), ID, name, priority, duration, latency, total_duration, total_latency, number_of_executions);
	
	_channels[0]->send(&msg);
}

inline int Communicator::action(mavlink_message_t msg)
{
	switch(msg.msgid){
		case MAVLINK_MSG_ID_THREAD_INFO:{
			MicroOS::sendSystemRequest(THREADINFO);
			break; }
			
		case MAVLINK_MSG_ID_GPIO:{
			mavlink_gpio_t gpio;
			mavlink_msg_gpio_decode(&msg,&gpio);
		
			for(uint8_t k=0;k<4;k++){
				MicroOS::setGPinInt(k,gpio.gpio_int[k]);
				MicroOS::setGPinFloat(k,gpio.gpio_float[k]);
				MicroOS::setGPinFloat(k+4,gpio.gpio_float[k+4]);
			}
			break;}
			
		/*case INTERESTING_MSG_ID:{
			//do something with the message
			break; }
			
		case LESS_INTERESTING_MSG_ID:{
			//do something with the message
			break; }*/
	}
		
	return 0;
}
