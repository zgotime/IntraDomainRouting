#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"
#include "stdio.h"
#include "string.h"


const char* RoutingProtocolImpl::LS_UPDATE_ALARM="LS";

const char* RoutingProtocolImpl::DV_UPDATE_ALARM = "DV";

const char* RoutingProtocolImpl::PING_ALARM = "PING";

const char* RoutingProtocolImpl::REFRESH_ALARM = "UPDATE";



RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
	sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
  
  delete[] port_status_list;
  
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
	// Passing the three arguments to the function
	this->num_ports = num_ports;
	this->router_id = router_id;
	this->protocol_type = protocol_type;
	
	port_status_list = new Port_Status[num_ports];
	
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
	sys->set_alarm(this,0,(void*)PING_ALARM);
 
	
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
  /* Get the claimed size of the packet */
  unsigned short packet_size =(unsigned short) ntohs(*(unsigned short*)((char*)packet+2));
  
  /* Compare the size value in the packet with the method input value */
  
  if(!packet_size==size){
	cerr << "RECV ERROR: Router: " << router_id << " received packet with wrong packet size at " <<sys->time()/1000.0 <<endl;
	free(packet);
	return;
  }
  
  char packet_type = *(char*) packet;
  cout << "RECV: Router: " << router_id << " received packet at " << sys->time()/1000.0<<endl;
  cout << "Packet size: "<< packet_size << endl;
  cout << "Packet type: " << (unsigned short)packet_type << endl;
  
  switch(packet_type){
	case DATA:
		handle_data_packet();
		break;
	case PING:
		handle_ping_packet(port, packet,size);
		break;
	case PONG:
		handle_pong_packet(port, packet);
		break;
	case LS:
		handle_ls_packet();
		break;
	case DV:
		handle_dv_packet();
		break;
	default:
		handle_invalid_packet();
		break;
  }
  
}

// add more of your own code


void RoutingProtocolImpl::handle_ping_alarm(){
	/* The packet size */
	unsigned short ping_packet_size = 12;
	
	/* Send to all of its ports */
	for(int i=0;i<num_ports;i++){
		char* packet = (char*)malloc(ping_packet_size);
		/* First 1 byte for the type */
		*(ePacketType*) packet = PING;
		/* Second 1 byte reserved ... */
		
		/* Third 2 bytes for the packet size */
		*(unsigned short*) (packet+2) = (unsigned short) htons(ping_packet_size); 
	
		/* Fourth 2 bytes for the router id */
		*(unsigned short*) (packet+4) = (unsigned short) htons(router_id);
		
		/* Fifth(last) 4 bytes for the PING time data */
		* (unsigned int*) (packet+8) = (unsigned int) htonl(sys->time());
		
		sys->send((unsigned short)i,packet,ping_packet_size);
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

void RoutingProtocolImpl::handle_data_packet(){

}

void RoutingProtocolImpl::handle_ping_packet(unsigned short port, void* packet, unsigned short size){
	/* Change the packet type to PONG */
	*(ePacketType*) packet = PONG;
	/* Move the source id to dest id in packet */
	memcpy((char*)packet+4,(char*)packet+6,2);
	/* Move the router's id to the source id */
	*(unsigned short*)((char*)packet+4) = (unsigned short) htons(router_id);
	/* Send the pong packet back to the sender */
	sys->send(port,packet,size);
}

void RoutingProtocolImpl::handle_pong_packet(unsigned short port, void* packet){
	/* Check if the PONG packet belongs to the router */
	if(!*(unsigned short*)((char*)packet+6)== router_id){
		cerr<< "RECV PONG ERROR: Router: "<<router_id<< "received PONG packet with wrong destination router ID at time: " << sys->time()/1000.0<<endl;
		free(packet);
		return;
	}

	unsigned short neighbor_router_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet+4));
	unsigned int time_stamp = (unsigned int) ntohl(*(unsigned int*)((char*)packet+8));
	
	Port_Status port_s = port_status_list[port];
	port_s.RTT = sys->time()-time_stamp;
	port_s.neighbor_router_id = neighbor_router_id;
	port_s.expire_time = sys->time()+PONG_MAX_TIMEOUT;
	
	free(packet);
}
	
void RoutingProtocolImpl::handle_ls_packet(){
}
	
void RoutingProtocolImpl::handle_dv_packet(){
}
	
void RoutingProtocolImpl::handle_invalid_packet(){
}