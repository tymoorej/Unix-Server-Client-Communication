#ifndef SWITCH_INCLUDE
#define SWITCH_INCLUDE

#include "includes.h"

struct switch_fd{
    int switch_number; // 0 for cont, -1 for keyboard
    int read_fd;
    int write_fd;
    switch_fd();
};

struct switch_signal_count{
    int admit;
    int ack;
    int add;
    int relay;
    int open;
    int query;

    switch_signal_count();
    void print_receiving();
    void print_transmitting();
};

struct switch_total_signals{
    struct switch_signal_count recieved;
    struct switch_signal_count transmitted;

    void print();
};

class Switch{
    struct sigaction oldAct, newAct;
    vector<struct switch_fd> switch_fd_list;
    vector<struct flow_element> switch_flow_table;
    struct switch_total_signals switch_signals;
    vector<struct instruction> instructions_waiting_queue;
    vector<struct instruction> traffic_file_queue;
    vector<int> sent_dest_ips;
    int switch_number, left, right, ip_range_lo, ip_range_hi, server_port_number, socket_fd;
    string traffic_file_name, server_address;
    bool delaying;
    time_t delay_start_time;
    int delay_length;



    void main(struct pollfd fdarray[], int number_of_fds);
    void setup(struct pollfd fdarray[]);
    void shutdown();
    void exit_program();
    
    void create_fifos();
    void build_poll_array(struct pollfd fdarray[]);
    void connect_to_server();
    string get_ip_address(const std::string& host);
    
    void send_message(struct message m, int target);
    void send_open_message();
    void send_query_message(struct instruction ins);
    void send_relay_message(int target, int source_ip, int dest_ip);
    
    void recieve_message_from_controller(message m);
    void recieve_message_from_switch(message m, int sw_rf);
    int get_switch_number_from_fd(int fd);
    
    void get_lines_from_traffic_file();
    void process_current_line_from_traffic_file(int tf_index);
    void process_instruction(struct instruction ins);
    int tf_get_sw(string sw);
    void process_waiting_queue();
    void clean_up_waiting_queue(vector<int> *indexes_to_remove);
    void update_delay();
    
    int get_fd_write(int target);
    void set_fd_write(int target, int fd);
    
    void initialize_flow_table();
    struct flow_element instruction_in_flow_table(struct instruction ins, int *found);
    bool rule_safe_to_add(struct flow_element rule);
    bool dest_ip_not_yet_sent(int dest_ip);
    void print_flow_table();
    
    
    public:
    Switch();
    void start(int switch_number, string traffic_file_name, int left, int right, int ip_range_lo, int ip_range_hi, string server_address, int server_port_number);
    void list_info();
};

#endif