#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"
#include "stdio.h"
#include "string.h"
#include <climits>


const char* RoutingProtocolImpl::LS_UPDATE_ALARM="LSUPDATE";

const char* RoutingProtocolImpl::DV_UPDATE_ALARM = "DVUPDATE";

const char* RoutingProtocolImpl::PING_ALARM = "PING";

const char* RoutingProtocolImpl::LS_REFRESH_ALARM = "LSREFRESH";

const char* RoutingProtocolImpl::DV_REFRESH_ALARM = "DVREFRESH";

const char* RoutingProtocolImpl::PORT_REFRESH_ALARM = "PORTREFRESH";

unsigned long RoutingProtocolImpl::LS_SEQUENCE = 0;


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
	

	/* Set the router_id to invalid -1 for every port */
	for(int i = 0; i<num_ports;i++){
		port_status_list[i].neighbor_router_id = -1;
	}
	
	
	/* Check the protocol type */
	if(protocol_type == P_DV){

		
		sys->set_alarm(this,DV_UPDATE_INTERVAL,(void *)DV_UPDATE_ALARM);
		sys->set_alarm(this,DV_REFRESH_RATE,(void *) DV_REFRESH_ALARM);
	}
	else if(protocol_type == P_LS){
		
		/* Set up the router's own LSP first*/
		LS_Info ls_i;
		ls_i.expire_time = sys->time() + LS_MAX_TIMEOUT; // Set expire time here so we can delete all entries in the current node when it expires
		ls_i.sequence = LS_SEQUENCE;
	
		ls_table[router_id] = ls_i;
		LS_SEQUENCE++; // inc sequence number every time ls_info changes
	
		sys->set_alarm(this,LS_UPDATE_INTERVAL,(void *) LS_UPDATE_ALARM);
		sys->set_alarm(this,LS_REFRESH_RATE,(void *)LS_REFRESH_ALARM);
		
	}
	else{
		handle_invalid_protocol_type();
	}
	
	/* Ping the message to the ports */
	handle_ping_alarm();
	/* Do a refresh check for every port */
	sys->set_alarm(this,1000,(void*)PORT_REFRESH_ALARM);
 

 }

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code

	char* alarm_type = (char*) data;

	if(strcmp(alarm_type,PING_ALARM)==0){
		handle_ping_alarm();
	}
	else if(strcmp(alarm_type,DV_UPDATE_ALARM)==0){
		handle_dv_update_alarm();
	}
	else if(strcmp(alarm_type,LS_UPDATE_ALARM)==0){
		handle_ls_update_alarm();
	}
	else if(strcmp(alarm_type,LS_REFRESH_ALARM)==0){
		handle_ls_refresh_alarm();
	}
	else if(strcmp(alarm_type,DV_REFRESH_ALARM)==0){
		handle_dv_refresh_alarm();
	}
	else if(strcmp(alarm_type,PORT_REFRESH_ALARM)==0){
		handle_port_refresh_alarm();
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
		handle_data_packet(port, packet, size);
		break;
	case PING:
		handle_ping_packet(port, packet,size);
		break;
	case PONG:
		handle_pong_packet(port, packet);
		break;
	case LS:
		handle_ls_packet(port,packet,size);
		break;
	case DV:
		handle_dv_packet(port, packet,size);
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
	
		/* Fourth 2 bytes for the source id */
		*(unsigned short*) (packet+4) = (unsigned short) htons(router_id);
		
		/* Fifth 2 bytes for the dest id which is left blank in Ping case*/
		
		/* Sixth(last) 4 bytes for the PING time data */
		* (unsigned int*) (packet+8) = (unsigned int) htonl(sys->time());
		
		sys->send((unsigned short)i,packet,ping_packet_size);
		cout << "Router: " << router_id << " send PING at " << sys->time() << endl;
	}


	/* Iteratively ping */
	sys->set_alarm(this,PING_INTERVAL,(void*)PING_ALARM);

}

void RoutingProtocolImpl::handle_ls_update_alarm(){

}

void RoutingProtocolImpl::handle_dv_update_alarm(){
	/* The packet size */
	unsigned short dv_update_packet_size = 8+(unsigned short)dv_table.size();
	
	/* Send to all of its destinations */
	for(int i =0;i<num_ports;i++){
		Port_Status port_s = port_status_list[i];
		if(port_s.neighbor_router_id>=0){

			char* packet = (char*) malloc(dv_update_packet_size);
			
			/* First 1 byte */
			*(ePacketType*) packet = DV;
			/* Second 1 byte reserved.. */
			
			/* Third 2 bytes for the packet size */
			*(unsigned short*) (packet+2) = (unsigned short) htons(dv_update_packet_size); 
		
			/* Fourth 2 bytes for the source id */
			*(unsigned short*) (packet+4) = (unsigned short) htons(router_id);
			
			/* Fifth 2 bytes for the dest id */
			*(unsigned short*) (packet+6) = (unsigned short) htons(port_s.neighbor_router_id);
		
			/* Sixth for data */
			// Left for change using memcpy to save iteration
			int j = 6;
			for(std::map<unsigned short, DV_Info>::iterator it=dv_table.begin();it!=dv_table.end();it++){
				*(unsigned short*) (packet+j) = (unsigned short) htons(it->first);
				*(unsigned short*) (packet+j+2) =(unsigned short) htons(it->second.cost);
				j = j+4;
			}
			
			sys->send((unsigned short)i,packet,dv_update_packet_size);
		}
	}
	
	sys->set_alarm(this,DV_UPDATE_INTERVAL,(void*)DV_UPDATE_ALARM);
	
}

void RoutingProtocolImpl::handle_dv_refresh_alarm(){
	for(std::map<unsigned short, DV_Info>::iterator it = dv_table.begin();it!=dv_table.end();it++){
		unsigned short current_router = it->first;
		DV_Info current_info = it->second;
		if((current_info.expire_time-sys->time())<0){
			cout<< "Router: "<<current_router<<" time-out"<<endl;
			/* Notify the neighbor about this time out for poison reverse*/
			current_info.cost = USHRT_MAX;
			dv_stack.push(current_info);
			/* Remove from DV table */
			dv_table.erase(current_router);
		}
	}
	/* Notify */
	handle_dv_stack();

	sys->set_alarm(this,DV_REFRESH_RATE,(void*)DV_REFRESH_ALARM);
}

void RoutingProtocolImpl::handle_ls_refresh_alarm(){
}

void RoutingProtocolImpl::handle_port_refresh_alarm(){
	
	/* Seperate the cases for DV and LS */
	if (protocol_type == P_DV){

		for (int i = 0; i < num_ports; i++){

			/* If it is a valid neighbor (!=-1) */

			if (port_status_list[i].neighbor_router_id >= 0){
				/* Compare time, erase the router id if exceed expire_time */
				if (port_status_list[i].expire_time - sys->time() < 0){
					/* Send Cost Infinity to prevent from poison reverse */
					DV_Info dv_s = dv_table[port_status_list[i].neighbor_router_id];
					dv_s.cost = USHRT_MAX;
					dv_stack.push(dv_s);

					/* If our path to neighbor is exactly neighbor itself, we erase it */
					if (dv_s.next_hop == port_status_list[i].neighbor_router_id){

						/* We remove it from our table*/
						dv_table.erase(port_status_list[i].neighbor_router_id);
					}

					/* Remove the neighbor */
					port_status_list[i].neighbor_router_id = -1;
				}
			}
		}

		/* Notify others about the change */

		handle_dv_stack();
	}
	else if (protocol_type == P_LS){
		for (int i = 0; i < num_ports;i++){

			/* If it is a valid neighbor (!=-1) */

			if (port_status_list[i].neighbor_router_id>=0){

				/* Check if expired */
				if (port_status_list[i].expire_time - sys->time()<0){
					/* Delete the node in entry, send the new LSP to everyone */
					ls_table[router_id].LSP.erase(port_status_list[i].neighbor_router_id);
					ls_table[router_id].sequence = LS_SEQUENCE;
					LS_SEQUENCE++; // The entry has changed... 
				}

			}

		}

		/* We've check the entry, refresh the time */
		ls_table[router_id].expire_time = sys->time() + LS_MAX_TIMEOUT;
		handle_ls_stack();
	}
	/* Do a refresh check for every port */
	sys->set_alarm(this,PING_REFRESH_RATE,(void*)PORT_REFRESH_ALARM);
}

void RoutingProtocolImpl::handle_invalid_alarm(){

}

void RoutingProtocolImpl::handle_invalid_protocol_type(){
}

void RoutingProtocolImpl::handle_data_packet(unsigned short port, void* packet, unsigned short size){

	/* Check if the DATA packet belongs to the router */
	unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 6));


	
	if (protocol_type == P_DV){
		/* The case when it is DV */
		/* When the port is originating from itself */
		if (port == USHRT_MAX){

			
			
			/* Check if the packet is actually for itself, wierd case */
			if (dest_id == router_id){
				free(packet);
				return;
			}
			else{
				/* If it is not itself, try to find the destination */
				if (dv_table.find(dest_id) == dv_table.end()){
					/* If the path is not stored in the table, cast it away */
					cout << "Router: " << router_id << "tried to originate DATA packet router ID at time: " << sys->time() / 1000.0 << endl;
					cout << "Router cannot find destination" << endl;
					free(packet);
				
				}
				else{


					

					/* It can get the next hop, then pass the data*/

					unsigned short next_hop = dv_table[dest_id].next_hop;

					
					
					/* Remember to change the source router ID */
					*(unsigned short*)((char *)packet + 4) = router_id;
					

					handle_send_data(hop_to_port[next_hop], packet, size);
				}

				/* Finished Handling with the data*/

				return;
			}
			

			

		}

		/* Else, it is receiving the data */

		if (dest_id == router_id){
			/* The router has recieved the DATA packet */
			cout << "Router: " << router_id << "received DATA packet router ID at time: " << sys->time() / 1000.0 << endl;
			free(packet);
			return;
		}

		if (dv_table.find(dest_id) == dv_table.end()){
			unsigned short next_hop = dv_table[dest_id].next_hop;
			handle_send_data(hop_to_port[next_hop], packet, size);
		}
		else{
			/* Else, the dv_table cannot find the destination*/
			cout << "Router: " << router_id << "received DATA packet router ID at time: " << sys->time() / 1000.0 << endl;
			cout << "Router cannot find destination " << endl;
			free(packet);
			return;
		}
	}
	else if(protocol_type == P_LS) {
		//TODO
	}

}


void RoutingProtocolImpl::handle_send_data(unsigned short port, void* packet, unsigned short size){
	cout << "port: " <<port <<"size: "<<size<< endl;
	sys->send(port, packet, size);
	
}


void RoutingProtocolImpl::handle_ping_packet(unsigned short port, void* packet, unsigned short size){

	


	
	/* Change the packet type to PONG */
	*(ePacketType*) packet = PONG;

	/* Set the size again, note that the packet type will cover the original size stored in data*/
	*(unsigned short*)((char*)packet + 2) = (unsigned short)htons(size);



	/* Move the source id to dest id in packet */
	memcpy((char*)packet+6,(char*)packet+4,2);


	
	/* Move the router's id to the source id */
	*(unsigned short*)((char*)packet+4) = (unsigned short) htons(router_id);
	/* Send the pong packet back to the sender */
	
	sys->send(port,packet,size);

}

void RoutingProtocolImpl::handle_pong_packet(unsigned short port, void* packet){
	/* Check if the PONG packet belongs to the router */
	unsigned short dest_id = *(unsigned short*)((char*)packet + 6);
	if(!dest_id== router_id){
		cerr<< "RECV PONG ERROR: Router: "<<router_id<< " received PONG packet with wrong destination router ID: "<<dest_id<<" at time: " << sys->time()/1000.0<<endl;
		free(packet);
		return;
	}

	unsigned short neighbor_router_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet+4));
	unsigned int time_stamp = (unsigned int) ntohl(*(unsigned int*)((char*)packet+8));
	
	port_status_list[port].RTT = sys->time() - time_stamp;
	port_status_list[port].neighbor_router_id = (int)neighbor_router_id;
	port_status_list[port].expire_time = sys->time() + PONG_MAX_TIMEOUT; // Potential overflow!?!? time could be very large!
	
	cout << "CHECKING PORTS: " << port_status_list[port].neighbor_router_id << endl;

	/* Update the neighbor router cost for DV and LS */
	if(protocol_type==P_DV){
		

		if(dv_table.find(neighbor_router_id)==dv_table.end()){

			/* If the neighbor is not in table */
			
			dv_table[neighbor_router_id].dest = neighbor_router_id;
			dv_table[neighbor_router_id].cost = port_status_list[port].RTT;
			dv_table[neighbor_router_id].expire_time = sys->time() + DV_MAX_TIMEOUT;
			dv_table[neighbor_router_id].next_hop = neighbor_router_id;
			

			DV_Info dv_s = dv_table[neighbor_router_id];

			dv_stack.push(dv_s);
			// Push the dv into the stack
		}
		else{
			/* Only push to stack if different */
			
			/* Refresh the time of the neighbor even if no changes are made */
			dv_table[neighbor_router_id].expire_time = sys->time() + DV_MAX_TIMEOUT;
			
			if (dv_table[neighbor_router_id].next_hop == neighbor_router_id){
				/* Replace the RTT if the stored hop is just the neighbor and they are different */
				if (dv_table[neighbor_router_id].cost != port_status_list[port].RTT){
					dv_table[neighbor_router_id].cost = port_status_list[port].RTT;

					DV_Info dv_s = dv_table[neighbor_router_id];
					// Push to stack
					dv_stack.push(dv_s);
				}
				
			}
			else if (dv_table[neighbor_router_id].cost > port_status_list[port].RTT){
				/* The cost is lower than stored cost, replace it and renew the hop */
				dv_table[neighbor_router_id].cost = port_status_list[port].RTT;
				dv_table[neighbor_router_id].next_hop = neighbor_router_id;

				// Push to stack
				DV_Info dv_s = dv_table[neighbor_router_id];
				dv_stack.push(dv_s);
			}
			// We do not care if the RTT is higher than stored cost
		}
		

		/* Store/Renew the mapping for neighbor to port */
		hop_to_port[neighbor_router_id] = port;

		/* Notify other nodes about the new neighbor change */
		handle_dv_stack();
	}
	else if(protocol_type==P_LS){
		// TO DO
		/* Check if the neighbor node is in the ls table/ own entry */
		if (ls_table[router_id].LSP.find(neighbor_router_id) == ls_table[router_id].LSP.end()){
			/* It does not contain the LSP, add to it */
			ls_table[router_id].LSP[neighbor_router_id] = port_status_list[port].RTT;
			ls_table[router_id].expire_time = sys->time()+LS_MAX_TIMEOUT; // Refresh the entry though it belongs to the router itself
			ls_table[router_id].sequence = LS_SEQUENCE;
			LS_SEQUENCE++; // Update the sequence, it has changed
			ls_stack.push(ls_table[router_id]); // Push to the stack for the handler to handle
		}
		else{
			/* Get the cost then decide if we want to change what is stored inside entry */
			unsigned short stored_cost = ls_table[router_id].LSP[neighbor_router_id];
			if (stored_cost != port_status_list[port].RTT){
				/* In this case the stored cost should be updated (change in RTT) */
				ls_table[router_id].LSP[neighbor_router_id] = port_status_list[port].RTT;
				ls_table[router_id].expire_time = sys->time() + LS_MAX_TIMEOUT;
				ls_table[router_id].sequence = LS_SEQUENCE;
				LS_SEQUENCE++;

				ls_stack.push(ls_table[router_id]); // Push to the stack for the handler to handle
			}

			// Else, nothing happens, same entry
		}

		handle_ls_stack();

	}
	
	free(packet);
}

void RoutingProtocolImpl::handle_dv_stack(){

	/* If there is no change, no need to send data */
	if (dv_stack.empty()){
		return;
	}
	
	
	unsigned short stack_size = (unsigned short) dv_stack.size()*4;
	char * stack_data = (char *) malloc(stack_size);
	int i = 0;
	while(!dv_stack.empty()){
		DV_Info dv_i = dv_stack.top();
		unsigned short dest = dv_i.dest;
		unsigned short cost = dv_i.cost;
		*(unsigned short*) (stack_data+i) = dest;
		*(unsigned short*) (stack_data+i+2) = cost;
		dv_stack.pop();
		i = i+4;
	}
	
	for(int j = 0; j<num_ports;j++){
		Port_Status port_s = port_status_list[j];
		if(port_s.neighbor_router_id!=-1){
			char * packet = (char *) malloc(stack_size+8);
			
			/* First 1 byte */
			*(ePacketType*) packet = DV;
			/* Second 1 byte reserved.. */
			
			/* Third 2 bytes for the packet size */
			*(unsigned short*) (packet+2) = (unsigned short) htons(stack_size+8); 
		
			/* Fourth 2 bytes for the source id */
			*(unsigned short*) (packet+4) = (unsigned short) htons(router_id);
			
			/* Fifth 2 bytes for the dest id */
			*(unsigned short*) (packet+6) = (unsigned short) htons(port_s.neighbor_router_id);
		
			/* Sixth for data, copy the */
			memcpy(packet+8,stack_data,stack_size);
			
			/* Send the packet */
			sys->send((unsigned short)j,packet,stack_size+8);
		}
	}
	
	/* After sending the data we free the original data */
	free(stack_data);

}

	
void RoutingProtocolImpl::handle_ls_packet(unsigned short port, void* packet, unsigned short size){

	/* Get the source ID */
	unsigned short source_router_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
	
	/* Get the sequence ID */
	unsigned int sequence = (unsigned int) ntohs(*(unsigned int*)((char*)packet +8)) ;

	/* Get the number of entries in the LS packet*/
	int num_ls_info = (int)((size - 12) / 4);
	
	
	/* Loop through all the entries in the packet and update the dv_info */
	for (int i = 0; i< num_ls_info; i++){
		// TODO!
		unsigned short node_id = (unsigned short)ntohs(*(unsigned short*)((char*) packet+12+i*4)); // Node id in packet
		unsigned short node_cost = (unsigned short)ntohs(*(unsigned short*)((char*)packet+14+i*4)); // Node cost to the node id

		LS_Info ls_i;
		ls_i.expire_time = sys->time() + LS_MAX_TIMEOUT;
		ls_i.LSP[node_id] = node_cost;
		ls_i.sequence = LS_SEQUENCE;

		if (ls_table.find(source_router_id)==ls_table.end()){
			/* The current entry is not in the table, store it */
			ls_table[source_router_id] = ls_i;
			LS_SEQUENCE++; // Used one sequence number
		}
		else{
			unsigned short stored_sequence = ls_table[source_router_id].sequence;
			if (stored_sequence!=sequence){
				/* In this case, they are two different sequences so we have to change it */
				ls_table[source_router_id] = ls_i;
				LS_SEQUENCE++; // Used one sequence number
			}

		}

	}


	/* After we have stored the packet information, we should forward towards all other ports */
	for (int j = 0; j < num_ports; j++){
		/* We would forward the packet to every other ports except for the incoming port */
		if (j != port){
			void* forward_packet = malloc(size);
			memcpy(forward_packet, packet, size);
			sys->send(j,packet,size);
		}

	}

	/* Free the packet sent to us */
	free(packet);

}


void RoutingProtocolImpl::handle_ls_stack(){
	
	/* Do nothing if the stack is empty */
	if (ls_stack.empty()){
		return;
	}


	/* Actually, ls_stack should just contain one entry most (only self-entry) */
	if (ls_stack.size() != 1){
		cerr << "LS Stack error: More than one entry in the stack! " << endl;
	}

	/* Get the LSP List */
	LS_Info ls_i = ls_stack.top();

	/* Get the sequence number */
	unsigned int sequence = ls_i.sequence;

	/* Get the entry size containing data */
	unsigned short entry_size = ls_i.LSP.size()*4;
	
	/* +4 for the sequence number, we need to put it in */
	char * stack_data = (char *)malloc(entry_size+4);
	
	/* The map in the LSP */
	map<unsigned short, unsigned short> LSP_map = ls_i.LSP;
	
	/* Recording the loop num */
	int i = 0;
	/* Loop through the map and put them into the data to send */
	for (std::map<unsigned short, unsigned short>::iterator it = LSP_map.begin(); it != LSP_map.end();it++){
		*(unsigned short*)(stack_data + i * 4) = (unsigned short)htons(it->first);
		*(unsigned short*)(stack_data + 2 + i * 4) = (unsigned short)htons(it->second);
		i++;
	}
	
	/* Send the update to every other ports */
	for (int j = 0; j<num_ports; j++){
		Port_Status port_s = port_status_list[j];
		/* We need to fill the first 8 bytes */
		char * packet = (char *)malloc(entry_size + 8);

		/* First 1 byte */
		*(ePacketType*)packet = LS;

		/* Second 1 byte reserved.. */

		/* Third 2 bytes for the packet size */
		*(unsigned short*)(packet + 2) = (unsigned short)htons(entry_size + 8);

		/* Fourth 2 bytes for the source id */
		*(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);

		/* Fifth 2 bytes ignored */

		/* Sixth for data, copy the */
		memcpy(packet + 8, stack_data, entry_size);

		/* Send the packet */
		sys->send((unsigned short)j, packet, entry_size + 8);
		
	}

	/* Finished dealing with the entry */
	ls_stack.pop();

	/* After sending the data we free the original data */
	free(stack_data);


}

void RoutingProtocolImpl::handle_dv_packet(unsigned short port, void* packet, unsigned short size){
	/* Check if the DV packet belongs to the router */
	if(!*(unsigned short*)((char*)packet+6)== router_id){
		cerr<< "RECV DV ERROR: Router: "<<router_id<< "received DV packet with wrong destination router ID at time: " << sys->time()/1000.0<<endl;
		free(packet);
		return;
	}

	unsigned short neighbor_router_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet+4));
	int num_dv_info = (int)((size-8)/4);
	// TODO: potential null neighbor?
	unsigned short neighbor_cost = dv_table[neighbor_router_id].cost;
	
	
	/* Loop through all the entries in the packet and update the dv_info */
	for(int i =0; i< num_dv_info;i++){
		unsigned short dest_id = *((unsigned short *)((char*)packet+8+i*4));
		unsigned short cost = *((unsigned short*)((char*)packet+10+i*4));
		/* Add the new information into the table only if the cost is not infinity */

		/* We do not care about the distance to ourselves */
		if (dest_id == router_id){
			continue;
		}

		if(dv_table.find(dest_id)==dv_table.end()&& (cost!=USHRT_MAX)){
			/* The destination node is not in the table */
			DV_Info dv_s;
			dv_s.dest = dest_id;
			dv_s.cost = cost+neighbor_cost;
			dv_s.expire_time = sys->time()+DV_MAX_TIMEOUT;
			dv_s.next_hop = neighbor_router_id;
			
			dv_table[dest_id] = dv_s;
			
			/* Push to stack */
			dv_stack.push(dv_s);
		}
		else {
			
			
			DV_Info dv_s = dv_table[dest_id];
			
			/* Check if an infinity package for poison reverse */
			if (cost == USHRT_MAX){
				/* If the infinity edge goes through the current stored hop, delete it and notify others */
				if (dv_s.next_hop == neighbor_router_id){
					dv_s.cost = USHRT_MAX;
					dv_stack.push(dv_s);
					dv_table.erase(dest_id);
				}

				/* Else it does nothing with our current path*/
			}
			else{
				/* Refresh the dest node time even if no changes are made */
				dv_s.expire_time = sys->time() + DV_MAX_TIMEOUT;

				if (dv_s.next_hop == neighbor_router_id){
					/* Add to dv stack only if they are different cost */
					if (dv_s.cost != (cost + neighbor_cost)){
						dv_s.cost = cost + neighbor_cost;
						/* Push to stack */
						dv_stack.push(dv_s);
					}
				}
				else if (dv_table[dest_id].cost > (cost + neighbor_cost)){
					/* Select a new shorter path, change next_hop */
					dv_s.cost = cost + neighbor_cost;
					dv_s.next_hop = neighbor_router_id;

					/* Push stack */
					dv_stack.push(dv_s);
				}
			}
		}
	}
	
	handle_dv_stack();
	
	free(packet);

}
	
void RoutingProtocolImpl::handle_invalid_packet(){
}