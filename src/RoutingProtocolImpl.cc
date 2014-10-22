#include "RoutingProtocolImpl.h"

static const char* RoutingProtocolImpl::PING_ALARM = "PING";
static const char* RoutingProtocolImpl::DV_ALARM = "DV";
static const char* RoutingProtocolImpl::LS_ALARM = "LS";
static const char* RoutingProtocolImpl::UPDATE_ALARM = "UPDATE";





RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
	// Passing the three arguments to the function
	this->num_ports = num_ports;
	this->router_id = router_id;
	this->protocol_type = protocol_type;
	
	port_status = new unsigned int[num_ports];
	port_router_id = new int[num_ports];
	
	/* Check the protocol type */
	if(protocol_type == P_DV){
		sys->set_alarm(this,DV_UPDATE_INTERVAL,(void *)DV_UPDATE_ALARM);
		sys->set_alarm(this,DV_REFRESH_RATE,(void *) REFRESH_ALARM);
		
	}
	else if(protocol_type == P_LS){
		sys->set_alarm(this,LS_UPDATE_INTERVAL,(void *) LS_UPDATE_ALARM);
		sys->set_alarm(this,DV_REFRESH_RATE,(void *)REFRESH_ALARM);
		
	}
	else{
		handle_invalid_protocol_type();
	}
	
	/* Ping the message to the ports */
	sys->set_alarm(this,0,(void*)&PING_ALARM);
 
	
 }

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
  
	char* alarm_type = (char*) data;
	
	if(strcmp(alarm_type,PING_ALARM)==0){
		handle_ping_alarm();
	}
	else if(strcmp(alarm_type,DV_UPDATE_ALARM)==0){
		handle_dv_alarm();
	}
	else if(strcmp(alarm_type,LS_UPDATE_ALARM)==0){
		handle_ls_alarm();
	}
	else if(strcmp(alarm_type,REFRESH_ALARM)==0){
		handle_update_alarm();
	}
	else{
		handle_invalid_alarm();
	}
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
}

// add more of your own code


void RoutingProtocolImpl::handle_ping_alarm(){
	/* The packet size */
	
	unsigned short ping_packet_size = 12;
	
	/* Send to all of its ports */
	for(int i=0;i<num_ports;i++){
	
	
		sys->send();
	}
	/* Iteratively ping */
	sys->set_alarm(this,PING_INTERVAL,(void*)PING_ALARM);
}

void RoutingProtocolImpl::handle_ls_alarm(){

}

void RoutingProtocolImpl::handle_dv_alarm(){
}

void RoutingProtocolImpl::handle_update_alarm(){
}

void RoutingProtocolImpl::handle_invalid_alarm(){

}

void RoutingProtocolImpl::handle_invalid_protocol_type(){
}