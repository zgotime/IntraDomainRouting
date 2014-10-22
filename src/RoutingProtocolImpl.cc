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
	
 }

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
  
	char* alarm_type = (char*) data;
	
	if(strcmp(alarm_type,PING_ALARM)==0){
		handle_ping_alarm();
	}
	else if(strcmp(alarm_type,DV_ALARM)==0){
		handle_dv_alarm();
	}
	else if(strcmp(alarm_type,LS_ALARM)==0){
		handle_ls_alarm();
	}
	else if(strcmp(alarm_type,UPDATE_ALARM)==0){
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
}

void RoutingProtocolImpl::handle_ls_alarm(){

}

void RoutingProtocolImpl::handle_dv_alarm(){
}

void RoutingProtocolImpl::handle_update_alarm(){
}

void RoutingProtocolImpl::handle_invalid_alarm(){

}