#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"
#include "stdio.h"
#include "string.h"
#include <set>
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
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
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
		return;
	}
	/* Ping the message to the ports */
	handle_ping_alarm();
	/* Do a refresh check for every port */
	sys->set_alarm(this,1000,(void*)PORT_REFRESH_ALARM);
 }

void RoutingProtocolImpl::handle_alarm(void *data) {

	/* Check the alarm type and handle all the alarms */
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

	/* Get the claimed size of the packet */
	unsigned short packet_size =(unsigned short) ntohs(*(unsigned short*)((char*)packet+2));

	/* Compare the size value in the packet with the method input value */
	if(packet_size!=size){
		cerr << "RECV ERROR: Router: " << router_id << " received packet with wrong packet size at " <<sys->time()/1000.0 <<endl;
		free(packet);
		return;
	}
  
	char packet_type = *(char*) packet;
  
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
		handle_invalid_packet(packet);
		break;
	}
  
}

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
	}
	/* Iteratively ping */
	sys->set_alarm(this,PING_INTERVAL,(void*)PING_ALARM);
}

void RoutingProtocolImpl::handle_ls_update_alarm(){

	/* Emit another update alarm */
	sys->set_alarm(this, LS_UPDATE_INTERVAL, (void *) LS_UPDATE_ALARM);
	
	/* Get the number of entries of this router */
	unsigned short num_ls_info = (unsigned short) ls_table[router_id].LSP.size();

	/* If there is no neighbors, no need to send it */
	if (num_ls_info == 0){
		return;
	}

	/* Increment the sequence number for a new LSP */
	ls_table[router_id].sequence = LS_SEQUENCE;
	LS_SEQUENCE++;
	
	/* Get the sequence number */
	unsigned int sequence = ls_table[router_id].sequence;

	/* Malloc the forwarding packet (add the size of sequence and header stuff) */
	char* packet = (char*)malloc(12+num_ls_info*4);

	/* First set the packet type */
	*(ePacketType*)packet = LS;

	/* Then set the size */
	*(unsigned short*)(packet + 2) = (unsigned short)htons((12+num_ls_info*4));

	/* Set the source ID */
	*(unsigned short*)(packet + 4) = htons(router_id);

	/* Set the sequence number */
	*(unsigned int*)(packet + 8) = htonl(sequence);

	/* Loop through all entries in the packet and paste the LSP entries */
	int i = 0;

	for (std::map<unsigned short, unsigned short>::iterator it = ls_table[router_id].LSP.begin(); it != ls_table[router_id].LSP.end();it++){
		*(unsigned short*)(packet + 12 + i * 4) = htons(it->first);
		*(unsigned short*)(packet + 14 + i * 4) = htons(it->second);
		i++;
	}

	/* Flood the packet to all other ports of this node */
	for (int j = 0; j < num_ports; j++){
		/* For each port, allocate a new packet */
		void* port_packet = malloc(12+num_ls_info*4);
		memcpy(port_packet, packet, 12 + num_ls_info * 4);
		sys->send(j, port_packet, 12+num_ls_info*4);
	}

	/* Free the original packet for copy purpose */
	free(packet);
}

void RoutingProtocolImpl::handle_dv_update_alarm(){

	/* Push all the entries to dv_stack to send */
	for (std::map<unsigned short, DV_Info>::iterator it = dv_table.begin(); it != dv_table.end();it++){
		DV_Info dv_c = it->second;
		dv_stack.push(dv_c);
	}
	handle_dv_stack();
	
	sys->set_alarm(this,DV_UPDATE_INTERVAL,(void*)DV_UPDATE_ALARM);
}

void RoutingProtocolImpl::handle_dv_refresh_alarm(){

	/* Refresh all the dv entries in the table */
	std::map<unsigned short, DV_Info>::iterator it = dv_table.begin();
	while(it!=dv_table.end()){
		DV_Info current_info;

		current_info.expire_time = it->second.expire_time;
		if((current_info.expire_time<sys->time())){
			current_info.cost = USHRT_MAX;
			current_info.dest = it->second.dest;
			current_info.next_hop = it->second.dest;
			dv_stack.push(current_info);
			dv_table.erase(it++); // Erase the entry in table since it has expired
		} else {
			++it;
		}
	}

	/* Notify */
	handle_dv_stack();
	sys->set_alarm(this,DV_REFRESH_RATE,(void*)DV_REFRESH_ALARM);
}

void RoutingProtocolImpl::handle_ls_refresh_alarm(){

	/* Check for every LS entry's refresh time, erase it if any of those has expired */
	bool change_flag = false; 
	for (std::map<unsigned short, LS_Info>::iterator it = ls_table.begin(); it != ls_table.end();it++){
		/* Check if the entry for the router itself has expired, weird case */
		if (it->first==router_id){
			if (it->second.expire_time < sys->time())
			{
				ls_table[router_id].expire_time = sys->time() + LS_MAX_TIMEOUT;
				ls_table[router_id].sequence = LS_SEQUENCE;
				ls_table[router_id].LSP.clear(); // EMPTY THE NEIGHBORS
				LS_SEQUENCE++; // Different entry 
				change_flag = true;

			}
		}
		else{
			if (it->second.expire_time < sys->time()){
				/* Just simply erase it */
				ls_table.erase(it->first);
				change_flag = true;
			}

		}

	}
	if(change_flag==true){
		/* Recompute the path */
		handle_compute_ls_path();
	}
	sys->set_alarm(this,LS_REFRESH_RATE,(void*)LS_REFRESH_ALARM);
}

void RoutingProtocolImpl::handle_port_refresh_alarm(){

	/* Seperate the cases for DV and LS */
	if (protocol_type == P_DV){
		for (int i = 0; i < num_ports; i++){
		
			/* If it is a valid neighbor (!=-1) */
			if (port_status_list[i].neighbor_router_id >= 0){
				/* Compare time, erase the router id if exceed expire_time */
				if (port_status_list[i].expire_time < sys->time()){
					/* Send Cost Infinity to prevent from poison reverse */
					DV_Info dv_s = dv_table[(unsigned short) port_status_list[i].neighbor_router_id];
					dv_s.cost = USHRT_MAX;
					dv_stack.push(dv_s);
					dv_table.erase((unsigned short) port_status_list[i].neighbor_router_id);
					/* Remove all the paths whose next hop is the disconnected port */
					std::map<unsigned short, DV_Info>::iterator it=dv_table.begin();
					while (it != dv_table.end()) {
						if (it->second.next_hop == (unsigned short) port_status_list[i].neighbor_router_id) {
							/* Note that we could potentially remove neighbors, so remove them in port_status_list too */
							for (int k = 0; k < num_ports;k++){
								if (port_status_list[k].neighbor_router_id >= 0){
									if (it->second.dest == port_status_list[k].neighbor_router_id){
										port_status_list[k].neighbor_router_id = -1;
									}
								}
							}
							/* We remove it from our table*/
							dv_table.erase(it++);
						} else {
							++it;
						}
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
		bool change_flag = false;
		for (int i = 0; i < num_ports;i++){
			/* If it is a valid neighbor (!=-1) */
			if (port_status_list[i].neighbor_router_id>=0){
				/* Check if expired */
				if (port_status_list[i].expire_time < sys->time()){
					/* Delete the node in entry, send the new LSP to everyone */
					ls_table[router_id].LSP.erase((unsigned short) port_status_list[i].neighbor_router_id);
					ls_table[router_id].sequence = LS_SEQUENCE;
					LS_SEQUENCE++; // The entry has changed... 
					port_status_list[i].neighbor_router_id = -1;
					change_flag = true;
				}
			}
		}

		/* We've check the entry, refresh the time */
		ls_table[router_id].expire_time = sys->time() + LS_MAX_TIMEOUT;
		/* There is change in the LSP */
		if (change_flag==true){
			ls_stack.push(ls_table[router_id]);
			handle_compute_ls_path();
		}
		handle_ls_stack();
	}
	/* Do a refresh check for every port */
	sys->set_alarm(this,PING_REFRESH_RATE,(void*)PORT_REFRESH_ALARM);
}

void RoutingProtocolImpl::handle_invalid_alarm(){
	cerr << "Router ID: "<<router_id<<" invoked Invalid alarm at time "<< sys->time()/1000.0 << endl;
}

void RoutingProtocolImpl::handle_invalid_protocol_type(){
	/* Handle invalid protocol type */
	cerr << "Invalid protocol type LS or DV given " << endl;
}

void RoutingProtocolImpl::handle_data_packet(unsigned short port, void* packet, unsigned short size){

	/* Check if the DATA packet belongs to the router */
	unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 6));

	if (protocol_type == P_DV){
		/* The case when it is DV */
		/* When the port is originating from itself */
		if (port == USHRT_MAX){
			/* Check if the packet is actually for itself, weird case */
			if (dest_id == router_id){
				free(packet);
				return;
			}
			else{
				/* If it is not itself, try to find the destination */
				if (dv_table.find(dest_id) == dv_table.end()){
					/* If the path is not stored in the table, cast it away */
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
			free(packet);
			return;
		}
		if (dv_table.find(dest_id) != dv_table.end()){
			unsigned short next_hop = dv_table[dest_id].next_hop;
			handle_send_data(hop_to_port[next_hop], packet, size);
		}
		else{
			/* Else, the dv_table cannot find the destination*/
			free(packet);
			return;
		}
	}
	else if(protocol_type == P_LS) {
		/* The case when it is LS */
		/* When the port is originating from itself */
		if (port == USHRT_MAX){
			/* Check if the packet is actually for itself, wierd case */
			if (dest_id == router_id){
				free(packet);
				return;
			}
			else{
				/* If it is not itself, try to find the destination */
				if (ls_mapping.find(dest_id) == ls_mapping.end()){
					/* If the path is not stored in the ls_mapping, cast it away */
					free(packet);
				}
				else{
					/* It can get the next hop, then pass the data*/
					unsigned short next_hop = ls_mapping[dest_id];
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
			/* The router has received the DATA packet */
			free(packet);
			return;
		}
		if (ls_mapping.find(dest_id) != ls_mapping.end()){
			unsigned short next_hop = ls_mapping[dest_id];
			handle_send_data(hop_to_port[next_hop], packet, size);
		}
		else{
			/* Else, the dv_table cannot find the destination*/
			free(packet);
			return;
		}
	}
}


void RoutingProtocolImpl::handle_send_data(unsigned short port, void* packet, unsigned short size){
	/* Send the data to the destination */
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
	
		bool change_flag = false; // Nothing changed yet
		/* Check if the neighbor node is in the ls table/ own entry */
		if (ls_table[router_id].LSP.find(neighbor_router_id) == ls_table[router_id].LSP.end()){
			/* It does not contain the LSP, add to it */
			ls_table[router_id].LSP[neighbor_router_id] = port_status_list[port].RTT;
			ls_table[router_id].expire_time = sys->time()+LS_MAX_TIMEOUT; // Refresh the entry though it belongs to the router itself
			ls_table[router_id].sequence = LS_SEQUENCE;
			LS_SEQUENCE++; // Update the sequence, it has changed
			ls_stack.push(ls_table[router_id]); // Push to the stack for the handler to handle
			change_flag = true; // New entry in the table
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
				change_flag = true; // entry changed
			}
			// Else, nothing happens, same entry
		}
		/* Store/Renew the mapping for neighbor to port */
		hop_to_port[neighbor_router_id] = port;
		handle_ls_stack();
		if (change_flag == true){
			handle_compute_ls_path();
		}
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
	/* Loop through the stack and pop the entries out */
	while(!dv_stack.empty()){
		DV_Info dv_i = dv_stack.top();
		unsigned short dest = dv_i.dest;
		unsigned short cost = dv_i.cost;
		*(unsigned short*) (stack_data+i) = (unsigned short) htons (dest);
		*(unsigned short*) (stack_data+i+2) = (unsigned short) htons (cost);
		dv_stack.pop();
		i = i+4;
	}
	
	for(int j = 0; j<num_ports;j++){
		Port_Status port_s = port_status_list[j];
		if(port_s.neighbor_router_id>=0){
			char * packet = (char *) malloc(stack_size+8);
			
			/* First 1 byte */
			*(ePacketType*) packet = DV;
			/* Second 1 byte reserved.. */
			
			/* Third 2 bytes for the packet size */
			*(unsigned short*) (packet+2) = (unsigned short) htons(stack_size+8); 
		
			/* Fourth 2 bytes for the source id */
			*(unsigned short*) (packet+4) = (unsigned short) htons(router_id);
			
			/* Fifth 2 bytes for the dest id */
			*(unsigned short*) (packet+6) = (unsigned short) htons((unsigned short) port_s.neighbor_router_id);
		
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

	bool change_flag = false;

	/* Get the source ID */
	unsigned short source_router_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
	
	/* Get the sequence ID */
	unsigned int sequence = (unsigned int) ntohl(*(unsigned int*)((char*)packet +8)) ;

	/* Get the number of entries in the LS packet */
	int num_ls_info = (int)((size - 12) / 4);
	
	/* Corner case, the ls_packet could be coming from the router itself and is outdated! */
	if (source_router_id != router_id){

		/* Initialize the received LSP */
		LS_Info ls_i;
		ls_i.sequence = sequence;
		ls_i.expire_time = sys->time() + LS_MAX_TIMEOUT;
		/* Loop through all the entries in the packet and update the ls_info */
		for (int i = 0; i < num_ls_info; i++){
			unsigned short node_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 12 + i * 4)); // Node id in packet
			unsigned short node_cost = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 14 + i * 4)); // Node cost to the node id
			ls_i.LSP[node_id] = node_cost;
		}
		if (ls_table.find(source_router_id) == ls_table.end()){
			/* The current entry is not in the table, store it */
			ls_table[source_router_id] = ls_i;
			change_flag = true; // The entry is inserted
		}
		else{
			unsigned short stored_sequence = ls_table[source_router_id].sequence;
			if (stored_sequence < sequence){
				/* In this case, they are two different sequences so we have to change it */
				ls_table[source_router_id] = ls_i;
				change_flag = true; // The entry is new
			}
		}
	}
	
	/* Recompute if there is change in the data */
	if (change_flag == true){
		handle_compute_ls_path();
		/* After we have stored the packet information, we should forward towards all other ports */
		/* Only flood the packet when the sequence number is not previously seen. */
		for (int j = 0; j < num_ports; j++){
			/* We would forward the packet to every other ports except for the incoming port */
			if (j != port){
				char* forward_packet = (char*) malloc(size);
				memcpy(forward_packet, packet, size);
				sys->send(j,forward_packet,size);
			}
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
	/* Record the sequence number */
	*(unsigned int*)stack_data = (unsigned int)htonl (sequence);
	/* Recording the loop num */
	int i = 0;
	/* Loop through the map and put them into the data to send */
	for (std::map<unsigned short, unsigned short>::iterator it = LSP_map.begin(); it != LSP_map.end();it++){
		*(unsigned short*)(stack_data +4 + i * 4) = (unsigned short)htons(it->first);
		*(unsigned short*)(stack_data + 6 + i * 4) = (unsigned short)htons(it->second);
		i++;
	}
	
	/* Send the update to every other ports */
	for (int j = 0; j<num_ports; j++){
		Port_Status port_s = port_status_list[j];
		/* We need to fill the first 8 bytes */
		char * packet = (char *)malloc(entry_size + 12);

		/* First 1 byte */
		*(ePacketType*)packet = LS;

		/* Second 1 byte reserved.. */

		/* Third 2 bytes for the packet size */
		*(unsigned short*)(packet + 2) = (unsigned short)htons(entry_size + 12);

		/* Fourth 2 bytes for the source id */
		*(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);

		/* Fifth 2 bytes ignored */

		/* Sixth for data, copy the */
		memcpy(packet + 8, stack_data, entry_size+4);

		/* Send the packet */
		sys->send((unsigned short)j, packet, entry_size + 12);
	}

	/* Finished dealing with the entry */
	ls_stack.pop();

	/* After sending the data we free the original data */
	free(stack_data);
}


void RoutingProtocolImpl::handle_compute_ls_path(){

	/* Computes the ls path based on current info, uses Dijkstra's algorithm */
	
	/* Initialize the distance map relative to this router */
	std::map<unsigned short, unsigned short> distance_to_start;
	/* Initialize the previous node map */
	std::map<unsigned, unsigned> previous_node;

	/* Initialize the queue for unvisited nodes */
	std::set<unsigned short> unvisited_nodes;

	/* Initiaize the starting router */
	distance_to_start[router_id] = 0;

	/* Loop through all the nodes */
	for (std::map<unsigned short, LS_Info>::iterator it = ls_table.begin(); it != ls_table.end();it++){
		unsigned short node_v = it->first;
		/* If the node is not the source id */
		if (node_v != router_id){
			/* Set to Infinity */
			distance_to_start[node_v] = USHRT_MAX;
		}
		/* Add the node to unvisited nodes */
		unvisited_nodes.insert(node_v);
	}
	
	while (!unvisited_nodes.empty()){
		unsigned short min_node = *unvisited_nodes.begin();
		unsigned short min_cost = distance_to_start[min_node];
		/* Loop through the set and get the node with minimum distance */
		for (std::set<unsigned short>::iterator it = unvisited_nodes.begin(); it != unvisited_nodes.end();it++){
			unsigned short node_c = *it;
			if (distance_to_start[node_c] < min_cost){
				min_node = node_c;
				min_cost = distance_to_start[node_c];
			}
		}
		/* Remove the minimum node from the set */
		unvisited_nodes.erase(min_node);
		std::map<unsigned short, unsigned short> LSP_c = ls_table[min_node].LSP;
		for (std::map<unsigned short, unsigned short>::iterator it = LSP_c.begin(); it != LSP_c.end();it++){
			/* Only for the neighbors unvisited */
			unsigned short neighbor_node = it->first;
			if (unvisited_nodes.find(neighbor_node)!=unvisited_nodes.end()){
				/* Calculate the alternative cost */
				unsigned short alt_cost = distance_to_start[min_node] + ls_table[min_node].LSP[neighbor_node];
				if (distance_to_start[min_node] == USHRT_MAX) {
					/* If the minimum node is unreachable, the alt_cost might overflow and cause infinite
					 while loop getting the hop. */
					alt_cost = USHRT_MAX;
				}
				/* Compare with the stored value */
				if (alt_cost < distance_to_start[neighbor_node]){
					/* If the cost is smaller, then it means the neighbor node is better off choosing the path through min_ node */
					distance_to_start[neighbor_node] = alt_cost;
					/* Record this step */
					previous_node[neighbor_node] = min_node;
				}
			}
		}
	}

	/* We should clear all the mapping before */
	ls_mapping.clear();
	/* Get the next_hop for each destination according to previous_node set */
	for (std::map<unsigned short, unsigned short>::iterator it = distance_to_start.begin(); it != distance_to_start.end();it++){
		/* Check that it is not this router itself */
		if (it->first != router_id){
			/* Check that the distance is not infinity (not reachable) for disruption in links */
			if (it->second != USHRT_MAX){
				/* Initialize the next_hop to be infinity */
				unsigned short previous_hop = it->first;
				unsigned short next_hop = previous_node[previous_hop];
				/* Loop till the next hop is found */
				while (next_hop!=router_id){
					previous_hop = next_hop;
					next_hop = previous_node[previous_hop];
				}
				/* Revert one step back, it is the neighbor of this router */
				ls_mapping[it->first] = previous_hop;
			}
		}
	}
}

void RoutingProtocolImpl::handle_dv_packet(unsigned short port, void* packet, unsigned short size){

	/* Check if the DV packet belongs to the router */
	unsigned short dest_i = ntohs(*(unsigned short*)((char*)packet+6));
	/* If the dv packet does not belong to the router, cast it away */
	if (dest_i != router_id){
		cerr << "Router ID: " << router_id << " received a packet does not belong to it , its dest is " << dest_i << " at time " << sys->time() / 1000.0 << endl;
		free(packet);
		return;
	}
	unsigned short neighbor_router_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet+4));
	int num_dv_info = (int)((size-8)/4);
	/* POTENTIAL NULL NEIGHBOR! */
	if (dv_table.find(neighbor_router_id) == dv_table.end()){
		/* We dont know how to deal with it, cast away */
		free(packet);
		return;
	}
	/* Else, we can deal with it */
	unsigned short neighbor_cost = dv_table[neighbor_router_id].cost;
	/* Loop through all the entries in the packet and update the dv_info */
	for(int i =0; i< num_dv_info;i++){
	
		unsigned short dest_id = ntohs(*((unsigned short *)((char*)packet+8+i*4)));
		unsigned short cost = ntohs(*((unsigned short*)((char*)packet+10+i*4)));
		/* Add the new information into the table only if the cost is not infinity */

		/* We do not care about the distance to ourselves */
		if (dest_id == router_id){
			continue;
		}
		if(dv_table.find(dest_id)==dv_table.end()){
			if (cost == USHRT_MAX) {
				continue;
			}
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
			/* Check if an infinity package for poison reverse */
			if (cost == USHRT_MAX){
				DV_Info dv_s = dv_table[dest_id];
				/* If the infinity edge goes through the current stored hop, delete it and notify others */
				if (dv_s.next_hop == neighbor_router_id){
					dv_s.expire_time = dv_table[dest_id].expire_time;
					dv_s.next_hop = dv_table[dest_id].next_hop;
					dv_s.cost = USHRT_MAX;
					dv_s.dest = dv_table[dest_id].dest;
					dv_stack.push(dv_s);
					dv_table.erase(dest_id);
				}
				/* Else it does nothing with our current path*/
			}
			else{
				DV_Info dv_s= dv_table[dest_id];
				/* Refresh the dest node time even if no changes are made */
				dv_s.expire_time = sys->time() + DV_MAX_TIMEOUT;
				dv_table[dest_id].expire_time = dv_s.expire_time;
				if (dv_s.next_hop == neighbor_router_id){
					/* Add to dv stack only if they are different cost */
					if (dv_s.cost != (cost + neighbor_cost)){
						dv_s.cost = cost + neighbor_cost;
						dv_table[dest_id].cost = dv_s.cost; // Change the value in the table
						/* Push to stack */
						dv_stack.push(dv_s);
					}
				}
				else if (dv_table[dest_id].cost > (cost + neighbor_cost)){
					/* Select a new shorter path, change next_hop */
					dv_s.cost = cost + neighbor_cost;
					dv_table[dest_id].cost = dv_s.cost;
					dv_s.next_hop = neighbor_router_id;
					dv_table[dest_id].next_hop = dv_s.next_hop;
					/* Push stack */
					dv_stack.push(dv_s);
				}
			}
		}
	}
	handle_dv_stack();
	free(packet);
}
	
void RoutingProtocolImpl::handle_invalid_packet(void* packet){
	/* Free the invalid packet */
	cerr << "Router ID: " << router_id <<" received invalid packet at time "<< sys->time() / 1000.0 << endl;
	free(packet);
}