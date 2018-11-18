// This file contains the definitions of shared methods

#include "includes.h"
#include "shared.h"

// convert an int to a string
string int_to_string(int i){
    stringstream ss;
    ss << i;
    string str = ss.str();
    return str;
}

// convert a msg type to a string
string msg_type_to_string(enum MSG_TYPE type){
    if (type == OPEN){
        return "OPEN";
    }
    if (type == ACK){
        return "ACK";
    }
    if (type == QUERY){
        return "QUERY";
    }
    if (type == ADD){
        return "ADD";
    }
    if (type == RELAY){
        return "RELAY";
    }
    
}

// Convert the string input, into a vector of strings, split on spaces
void get_vector_input(vector<string> *split_input, string input){
    (*split_input).clear();
    istringstream iss(input);
    for(input; iss >> input; ){
        (*split_input).push_back(input);
    }
}

// print a flow element
void flow_element::print(){
    printf("(srcIP= %d-%d, destIP= %d-%d, action= %s:%d, pri= %d, pkCount= %d)\n", \
    scrIP_lo, scrIP_hi, destIP_lo, destIP_hi, actionType_to_string(actionType).c_str(), actionVal, pri, pktCount);
}

// convert a action type to string
string actionType_to_string(enum actionType type){
    if (type == FORWARD){
        return "FORWARD";
    }
    return "DROP";
}

void message::print(){
    if (this->type == OPEN){
        this->data.open_data.print();
    }
    else if (this->type == ACK){
        this->data.ack_data.print();
    }
    else if (this->type == QUERY){
        this->data.query_data.print();
    }
    else if (this->type == ADD){
        this->data.add_data.print();
    }
    else if (this->type == RELAY){
        this->data.relay_data.print();
    }
}

void Open_Data::print(){
    string port1, port2;

    if(this->left == -1){
        port1 = "null";
    }
    else{
        port1 = "sw" + int_to_string(left);
    }

    if(this->right == -1){
        port2 = "null";
    }
    else{
        port2 = "sw" + int_to_string(right);
    }

    printf(" [OPEN]:\n\t(port0= cont, port1= %s, port2= %s, port3= %d-%d)\n", \
    port1.c_str(), port2.c_str(), this->ip_range_lo, this->ip_range_hi);
}

void Ack_Data::print(){
    printf(" [ACK]\n");
}

void Query_Data::print(){
    printf(" [QUERY]:\n\theader= (srcIP= %d, destIP= %d)\n", this->source_ip, this->dest_ip);
}

void Add_Data::print(){
    printf(" [ADD]:\n\t");
    this->rule.print();
}

void Relay_Data::print(){
    printf(" [RELAY]:\n\theader= (srcIP= %d, destIP= %d)\n", \
    this->ins.source_ip, this->ins.dest_ip);
}
