// the switch file has all of the methods and data needed for the switch

#include "switch.h"
#include "shared.h"

Switch::Switch(){}

// The switch initialization function, setup the switch, run the main loop
// when done shutdown the switch.
void Switch::start(int switch_number, string traffic_file_name, int left, int right, int ip_range_lo, int ip_range_hi, string server_address, int server_port_number){
    this->switch_number = switch_number;
    this->traffic_file_name = traffic_file_name;
    this->left = left;
    this->right = right;
    this->ip_range_lo = ip_range_lo;
    this->ip_range_hi = ip_range_hi;
    this->server_address = server_address;
    this->server_port_number = server_port_number;

    int number_of_fds = 2; // controller + keyboard
    if(this->left !=-1){
        number_of_fds++;
    }
    if(this->right !=-1){
        number_of_fds++;
    }
    struct pollfd fdarray[number_of_fds];
    this->setup(fdarray);
    this->main(fdarray, number_of_fds);
    this->shutdown();
}

// The main switch loop. Polls all file descriptors to detect any incomming
// messages and handles them accordingly
void Switch::main(struct pollfd fdarray[], int number_of_fds){
    int rval;
    string line;

    int tf_index = 0;

    while (true){
        
        if (this->delaying){
            this->update_delay();
        }
        else{
            this->process_current_line_from_traffic_file(tf_index);
            tf_index++;
        }

        rval = poll(fdarray, number_of_fds, 10);
        if (rval > 0){
            for (int i=0; i <= number_of_fds; i++){
                if (fdarray[i].revents & POLLIN){
                    // stdin (keyboard)
                    if (fdarray[i].fd == STDIN_FILENO){
                        cin >> line;
                        cout << "Recieved " << line << " From Keyboard" << endl;
                        if (line == "exit"){
                            this->exit_program();
                        }
                        if (line == "list"){
                            this->list_info();
                        }
                    }
                    // switch or controller
                    else{
                        struct message m;
                        memset( (char *) &m, 0, sizeof(m) );

                        int read_val = read(fdarray[i].fd, (char*)&m, sizeof(m));

                        if(read_val < 0){
                            perror("Read Failed");
                            exit(1);
                        }

                        else if (read_val == 0){
                            printf("Lost connection to controller.\n");
                            close(fdarray[i].fd);
                            fdarray[i].events = 0;
                            this->exit_program();
                            exit(0);
                        }
                        else{
                            // controller
                            if (fdarray[i].fd == this->socket_fd){
                                this->recieve_message_from_controller(m);
                            }
                            else{ // switch
                                this->recieve_message_from_switch(m, get_switch_number_from_fd(fdarray[i].fd));
                            }
                            this->process_waiting_queue();
                        }
                    }
                }
            }
        }
    }
}

// Sets up the switch, changes the behaviour of the SIGUSR1 signal,
// initialize the flow table, create all needed fifos, build the poll array,
// send the open message to the controller, and load in the traffic file
void Switch::setup(struct pollfd fdarray[]){
    this->newAct.sa_handler = switch_handle_signal_USR1;
    sigaction(SIGUSR1, &this->newAct, &this->oldAct);
    cout << "Starting Switch " << this->switch_number << ". Switch pid: " << getpid() << endl;
    this->initialize_flow_table();
    this->create_fifos();
    this->connect_to_server();
    this->build_poll_array(fdarray);
    this->send_open_message();
    this->get_lines_from_traffic_file();
}

// Shutdown the switch, restore signal behaviour
void Switch::shutdown(){
    cout << "Switch Shutting Down" << endl;
    sigaction(SIGUSR1,&this->oldAct,&this->newAct);
}

// Calls list and then shuts down
void Switch::exit_program(){
    this->list_info();
    this->shutdown();
    exit(0);
}

// Prints the switch flow table and all recieved signals
void Switch::list_info(){
    this->print_flow_table();
    this->switch_signals.print();
}


// Create all fifos needed for communication to and from other switches
// opens all fifos the switch reads from but not fifos the switch 
// writes to. Stores these read fds in the switch_fd data structure
void Switch::create_fifos(){
    string first, second, third;
    int fd;

    if (this->left != -1){
        first = "fifo-" + int_to_string(this->switch_number) + "-" + int_to_string(this->left);
        second = "fifo-" + int_to_string(this->left) + "-" + int_to_string(this->switch_number);
        mkfifo(first.c_str(), 0666);
        mkfifo(second.c_str(), 0666);

        struct switch_fd fds = switch_fd();
        fd = open(second.c_str(), O_RDWR | O_NONBLOCK);
        fds.read_fd = fd;
        fds.switch_number = this->left;
        this->switch_fd_list.push_back(fds);

    }
    if (this->right != -1){
        first = "fifo-" + int_to_string(this->switch_number) + "-" + int_to_string(this->right);
        second = "fifo-" + int_to_string(this->right) + "-" + int_to_string(this->switch_number);
        mkfifo(first.c_str(), 0666);
        mkfifo(second.c_str(), 0666);

        struct switch_fd fds = switch_fd();
        fd = open(second.c_str(), O_RDWR | O_NONBLOCK);
        fds.read_fd = fd;
        fds.switch_number = this->right;
        this->switch_fd_list.push_back(fds);
    }
    struct switch_fd fds = switch_fd();

    // keyboard
    fds = switch_fd();
    fds.read_fd = STDIN_FILENO;
    fds.switch_number = -1;
    this->switch_fd_list.push_back(fds);
    
}

// builds the pollfd array which will hold all of the fds and thier events
void Switch::build_poll_array(struct pollfd fdarray[]){
    // controller
    fdarray[0].fd = this->socket_fd;
    fdarray[0].events= POLLIN;
    fdarray[0].revents= 0;

    int i = 1;
    for (vector<struct switch_fd>::iterator it = this->switch_fd_list.begin(); it != this->switch_fd_list.end(); ++it){
        fdarray[i].fd = it->read_fd;
        fdarray[i].events= POLLIN;
        fdarray[i].revents= 0;
        i++;
    }

}

// Converts the host name to an IP-address
string Switch::get_ip_address(const string &s) {
    hostent* hostname = gethostbyname(s.c_str());
    if(hostname)
        return string(inet_ntoa(**(in_addr**)hostname->h_addr_list));
    else{
        cout << "Invalid Server Name - Cannot be converted to IP Address." << endl;
        exit(0);
    }
}

// Connect the switch to the controller by creating
// a socket and connecting to the sever through the
// socket
void Switch::connect_to_server(){
    struct sockaddr_in client;
    
    this->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->socket_fd <= 0){
        perror("Creation of socket failed"); 
        exit(1); 
    }

    struct sockaddr_in serv;
    memset(&serv, '0', sizeof(serv)); 
    serv.sin_family = AF_INET; 
    serv.sin_port = htons(this->server_port_number); 

    inet_pton(AF_INET, get_ip_address(this->server_address).c_str(), &serv.sin_addr);

    if(connect(this->socket_fd, (struct sockaddr *)&serv, sizeof(serv)) < 0){
        perror("Socket Connection Failed");
        exit(1);
    }
}

// Given a message and target switch, send a message to that switch
// also opens the write fifo if not already open
// Or if the target is the controller send a message through the FIFO
void Switch::send_message(struct message m, int target){
    if (target == 0){
        printf("Transmitted (src= sw%d, dest= cont)", this->switch_number);
        write(this->socket_fd, (char*)&m, sizeof(m));
    }
    else{
        printf("Transmitted (src= sw%d, dest= sw%d)", this->switch_number, target);
        int fd = this->get_fd_write(target);
        if (fd == -1){
            string ds = "fifo-" + int_to_string(this->switch_number) + "-" + int_to_string(target);
            fd  = open(ds.c_str(), O_WRONLY | O_NONBLOCK);
            this->set_fd_write(target, fd);
        }
        write(fd, (char*)&m, sizeof(m));
    }
    m.print();
}

// Construct and send the OPEN message also incriment the 
// OPEN transmitted signal counter
void Switch::send_open_message(){
    struct message m;
    struct Open_Data data;
    memset((char *) &m, 0, sizeof(m));

    m.type = OPEN;
    data.switch_number = this->switch_number;
    data.left = this->left;
    data.right = this->right;
    data.ip_range_lo = this->ip_range_lo;
    data.ip_range_hi = this->ip_range_hi;
    m.data.open_data = data;
    this->send_message(m, 0);
    this->switch_signals.transmitted.open++;
}

// Construct and send the QUERY message to the controller, also incriment the 
// QUERY transmitted signal counter
void Switch::send_query_message(struct instruction ins){
    struct message m;
    struct Query_Data data;
    memset((char *) &m, 0, sizeof(m));

    m.type = QUERY;
    data.source_ip = ins.source_ip;
    data.dest_ip = ins.dest_ip;
    m.data.query_data = data;
    this->send_message(m, 0);
    this->switch_signals.transmitted.query++;
}

// Construct and send the RELAY message to a neigboring switch, also incriment the 
// RELAY transmitted signal counter
void Switch::send_relay_message(int target, int source_ip, int dest_ip){
    struct message m;
    struct Relay_Data data;
    struct instruction ins;
    memset((char *) &m, 0, sizeof(m));

    ins.source_ip = source_ip;
    ins.dest_ip = dest_ip;
    data.ins=ins;

    m.type = RELAY;
    m.data.relay_data = data;
    this->send_message(m, target);
    this->switch_signals.transmitted.relay++;
}


// Handle Received messages from controller, if type ACK only incriment counter,
// if of type add, add the rule to the flow table if the rule is safe (does not 
// conflict with a pre-existing rule)
// the switch recieves all messages through the socket
void Switch::recieve_message_from_controller(message m){
    printf("Received (src= cont, dest=sw%d)", this->switch_number);
    m.print();
    if (m.type == ACK){
        this->switch_signals.recieved.ack++;
        return;
    }
    if (m.type == ADD && this->rule_safe_to_add(m.data.add_data.rule)){
        this->switch_signals.recieved.add++;
        this->switch_flow_table.push_back(m.data.add_data.rule);
        return;
    }
}

// Handle Received messages from other switches, add the new instruction to the waiting queue
// after sending out a query if the switch does not know how to handle the instruction.
// If the switch knows how to handle the instruction, handle it immediatley.
void Switch::recieve_message_from_switch(message m, int sw_rf){
    printf("Received (src= %d, dest=sw%d)", sw_rf, this->switch_number);
    m.print();
    if(m.type == RELAY){
        this->switch_signals.recieved.relay++;

        struct instruction ins = m.data.relay_data.ins;
        this->process_instruction(ins);
    }
}

// get which switch number a fd belongs to
int Switch::get_switch_number_from_fd(int fd){
    for (int i=0; i < switch_fd_list.size(); i++){
        if (switch_fd_list[i].read_fd == fd){
            return switch_fd_list[i].switch_number;
        }
    }
    return -1;
}



// Get all lines from traffic file, performs error handling
// stores all lines in a data structure so it can be easily
// fed to switch line by line in each iteration
void Switch::get_lines_from_traffic_file(){
    string line;
    int tf_sw_num, tf_source, tf_dest;
    vector<string> split_line;

    ifstream traffic_file;
    traffic_file.open(this->traffic_file_name.c_str());

    int error_checker = -1;

    while(getline(traffic_file, line)) {
        if (line.at(0) == '#'){
            continue;
        }
        get_vector_input(&split_line, line);

        if (split_line.size()!=3){
            continue;
        }

        error_checker++;
        
        tf_sw_num = this->tf_get_sw(split_line[0]);

        if(strcmp(split_line[1].c_str(),"delay") == 0 || strcmp(split_line[1].c_str(),"Delay") == 0){
            tf_source = DELAY;
        }
        else{
            tf_source = atoi(split_line[1].c_str());
        }

        tf_dest = atoi(split_line[2].c_str());

        if(tf_sw_num != this->switch_number){
            continue;
        }

        struct instruction ins= instruction();
        ins.source_ip = tf_source;
        ins.dest_ip = tf_dest;
        this->traffic_file_queue.push_back(ins);
    }

    if (error_checker == -1){
        cout << "Invalid or empty traffic_file" << endl;
        exit(0);
    }
}

// Process a current line from the traffic file, and handles the delay
void Switch::process_current_line_from_traffic_file(int tf_index){
    if (tf_index >= this->traffic_file_queue.size()){
        return;
    }
    struct instruction ins = this->traffic_file_queue[tf_index];

    if (ins.source_ip == DELAY){
        time(&this->delay_start_time);
        this->delaying = true;
        this->delay_length = ins.dest_ip;
        printf("\n** Entering a delay period of %d msec\n\n",ins.dest_ip);
        return;
    }
    
    this->switch_signals.recieved.admit++;

    this->process_instruction(ins);
}

// Process a current instruction, admit the line, query the controller if nessesary
// if we can't resolve it immideatly we must add it to the waiting queue, otherwise resolve it 
void Switch::process_instruction(struct instruction ins){
    int found = -1;
    struct flow_element element = this->instruction_in_flow_table(ins, &found);

    if (found == -1){
        if(this->dest_ip_not_yet_sent(ins.dest_ip)){
            this->send_query_message(ins);
            this->sent_dest_ips.push_back(ins.dest_ip);
        }
        this->instructions_waiting_queue.push_back(ins);
        return;
    }
    else{
        element.pktCount++;
        this->switch_flow_table[found] = element;
        if (element.actionVal == 3){
            return;
        }
        if (element.actionVal == 1){
            this->send_relay_message(this->left, ins.source_ip, ins.dest_ip);
            return;
        }
        if (element.actionVal == 2){
            this->send_relay_message(this->right, ins.source_ip, ins.dest_ip);
            return;
        }
    }
}

// Get the sw# from the transfer file
int Switch::tf_get_sw(string sw){
    if ((sw.length() == 3 && isdigit(sw.at(2)))){
        return sw.at(2) - '0';
    }
    else if(sw == "null"){
        return -1;
    }
    else{
        cout << "Invalid Field in Traffic file" << endl;
        exit(0);
    }
}

// Iterate through the waiting queue and for each instruction determine if it can
// be executed, if it can then execute it (either just incriment it or incriment it
// and relay it) then remove it from the queue. Otherwise keep it in the queue
void Switch::process_waiting_queue(){
    vector<int> indexes_to_remove;
    for (int i=0; i < this->instructions_waiting_queue.size(); i++){
        struct instruction ins = this->instructions_waiting_queue[i];
        int found = -1;
        struct flow_element element = this->instruction_in_flow_table(ins, &found);
        if (found == -1){
            continue;
        }
        else{
            indexes_to_remove.push_back(i);
            element.pktCount++;
            this->switch_flow_table[found] = element;
            if (element.actionVal == 1){
                this->send_relay_message(this->left, ins.source_ip, ins.dest_ip);
            }
            if (element.actionVal == 2){
                this->send_relay_message(this->right, ins.source_ip, ins.dest_ip);
            }
        }
    }

    sort(indexes_to_remove.begin(), indexes_to_remove.end(), greater<int>());
    this->clean_up_waiting_queue(&indexes_to_remove);
}

// Takes in a reverse-sorted array of indexes and removes those indexes from the instruction
// waiting queue
void Switch::clean_up_waiting_queue(vector<int> *indexes_to_remove){
    for (int i=0; i < (*indexes_to_remove).size(); i++){
        this->instructions_waiting_queue.erase(this->instructions_waiting_queue.begin() + (*indexes_to_remove)[i]);
    }
}

// Gets the write_fd of a specific switch attached to the current switch
int Switch::get_fd_write(int target){
    for (vector<struct switch_fd>::iterator it = this->switch_fd_list.begin(); it != this->switch_fd_list.end(); ++it){
        if (it->switch_number == target){
            return it->write_fd;
        }
    }
    return -1;
}

// Sets the write_fd of a specific switch attached to the current switch
void Switch::set_fd_write(int target, int fd){
    for (vector<struct switch_fd>::iterator it = this->switch_fd_list.begin(); it != this->switch_fd_list.end(); ++it){
        if (it->switch_number == target){
            it->write_fd = fd;
            return;
        }
    }
}

// Determines if the switch should still be in a delay state
void Switch::update_delay(){
    time_t now;
    time(&now);
    double difference = difftime(now, this->delay_start_time);

    if ((difference * 1000) >= delay_length){
        printf("\n**Delay Period Ended\n\n");
        this->delaying = false;
        this->delay_length = 0;
        this->delay_start_time = 0;
    }

}

// Initialize the switch's flow table by adding the default entry to the table
void Switch::initialize_flow_table(){
    struct flow_element element;
    element.scrIP_lo = 0;
    element.scrIP_hi = MAXIP;
    element.destIP_lo = this->ip_range_lo;
    element.destIP_hi = this->ip_range_hi;
    element.actionType = FORWARD;
    element.actionVal = 3;
    element.pri = MINPRI;
    element.pktCount = 0;
    this->switch_flow_table.push_back(element);
}

// Searches to see if a instruction is in the flow table, if so then it returns the element
// also passed in a found pointer, the passed pointer is set to the index of the rule if 
// the rule is found, -1 otherwise
struct flow_element Switch::instruction_in_flow_table(struct instruction ins, int *found){
    bool source_ip_range_ok, dest_ip_range_ok;
    struct flow_element element;
    for (int i=0; i < this->switch_flow_table.size(); i++){
        element = this->switch_flow_table[i];
        source_ip_range_ok = (ins.source_ip >= element.scrIP_lo) && (ins.source_ip <= element.scrIP_hi);
        dest_ip_range_ok = (ins.dest_ip >= element.destIP_lo) && (ins.dest_ip <= element.destIP_hi);
        if (source_ip_range_ok && dest_ip_range_ok){
            (*found) = i;
            return element;
        }
    }
    (*found) = -1;
}

// Determins if a rule is safe to add to the flow table (does not overlap with another rule)
bool Switch::rule_safe_to_add(struct flow_element rule){
    int hi = rule.destIP_hi;
    int lo = rule.destIP_lo;
    struct flow_element element;
    for (int i=0; i < this->switch_flow_table.size(); i++){
        element = this->switch_flow_table[i];
        if (lo >= element.destIP_lo && lo <= element.destIP_hi){
            return false;
        }
        if (hi >= element.destIP_lo && hi <= element.destIP_hi){
            return false;
        }
    }
    return true;
}

// Determine if a destination ip has already been sent to the controller
// in a query
bool Switch::dest_ip_not_yet_sent(int dest_ip){
    for (int i=0; i < this->sent_dest_ips.size(); i++){
        if (this->sent_dest_ips[i] == dest_ip){
            return false;
        }
    }
    return true;
}

// Prints the switch flow table
void Switch::print_flow_table(){
    cout << "Flow Table:" << endl;
    int i = 0;
    for (vector<struct flow_element>::iterator it = this->switch_flow_table.begin(); it != this->switch_flow_table.end(); ++it){
        cout << "[" << i << "] ";
        it->print();
        i++;
    }
}


/* This group of functions relate to a switch's signal counts
*/
switch_fd::switch_fd(){
    this->read_fd = -1;
    this->write_fd = -1;
}

switch_signal_count::switch_signal_count(){
    this->open=0;
    this->ack=0;
    this->query=0;
    this->add=0;
};

void switch_signal_count::print_receiving(){
    cout << "ADMIT: " << this->admit << ",   ";
    cout << "ACK: " << this->ack << ",   ";
    cout << "ADD: " << this->add << ",   ";
    cout << "RELAY: " << this->relay << endl;
};

void switch_signal_count::print_transmitting(){
    cout << "OPEN: " << this->open << ",   ";
    cout << "QUERY: " << this->query << ",   ";
    cout << "RELAY: " << this->relay << endl;
};

void switch_total_signals::print(){
    cout << "Packet Stats:" << endl;
    cout << "\tRecieved: ";
    this->recieved.print_receiving();
    cout << "\tTransmitted: ";
    this->transmitted.print_transmitting();
};