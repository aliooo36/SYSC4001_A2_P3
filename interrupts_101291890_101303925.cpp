/**
 *
 * @file interrupts.cpp
 * @author Muhammad Ali 101291890
 * @author Gregory Horvat 101303925
 *
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;
    static unsigned int next_pid = 1;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here

            // execution output
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Cloning the PCB\n";
            current_time += duration_intr;

            // creating child PCB with its own partition
            PCB child(next_pid, current.PID, current.program_name, current.size, -1);
            allocate_memory(&child);
            next_pid++;

            execution += std::to_string(current_time) + ", 0, " + scheduler();
            current_time += 0;

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;            

            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the chile (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)

            // Creating the child wait queue
            std::vector<PCB> child_wait_queue = wait_queue;
            child_wait_queue.push_back(current);
            
            // Adding system_status output
            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(child, child_wait_queue);
            system_status += "\n";
            
            // Running the child trace recursively
            auto [child_exec, child_status, child_time] = simulate_trace(
                child_trace,
                current_time,
                vectors,
                delays,
                external_files,
                child,
                child_wait_queue
            );

            execution += child_exec;
            system_status += child_status;
            current_time = child_time;
            
            // Freeing the child's memory after completion
            free_memory(&child);

            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here

            // search for the program in external_files
            unsigned int program_size = 0;
            bool found = false;
            for (const auto& file : external_files)
            {
                if (file.program_name == program_name)
                {
                    // program found
                    program_size = file.size;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // execution output for no program found
                execution += std::to_string(current_time) + ", 1, ERROR: Program " + program_name + " not found in external files\n";
                current_time += 1;
            }
            else
            {
                // execution output logic for if the program is found
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(program_size) + " MB large\n";
                current_time += duration_intr;

                // freeing current partition to find where executable will fit
                free_memory(&current);
                execution += std::to_string(current_time) + ", 1, free current partition\n";
                current_time += 1;

                // update PCB
                current.program_name = program_name;
                current.size = program_size;
                current.partition_number = -1;

                // allocate memory
                if (allocate_memory(&current))
                {
                    execution += std::to_string(current_time) + ", 1, allocate partition " + std::to_string(current.partition_number) + " for " + program_name + "\n";
                    current_time += 1;
                    // requirement h, load program from disk to RAM
                    int load_time = program_size * 15; // assumption in assignment (15ms for every MB)
                    execution += std::to_string(current_time) + ", " + std::to_string(load_time) + ", loading " + program_name + " into memory\n";
                    current_time += load_time;

                    // requirement i
                    execution += std::to_string(current_time) + ", 1, partition " + std::to_string(current.partition_number) + " marked as occupied\n";
                    current_time += 1;

                    // requirement j, log PCB update
                    execution += std::to_string(current_time) + ", 1, update PCB with new program info\n";
                    current_time += 1;

                    // requirment k, call scheduler
                    execution += std::to_string(current_time) + ", 1, " + scheduler();
                    current_time += 1;

                    execution += std::to_string(current_time) + ", 1, IRET\n";
                    current_time += 1;                    
                }
                else
                {
                    // no partition found
                    execution += std::to_string(current_time) + ", 1, ERROR: no partition available for " + program_name + " (size: " + std::to_string(program_size) + "MB)\n";
                    current_time += 1;
                }
            }

            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            exec_trace_file.close();

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

            // record system status if program was found and allocated
            if (found && current.partition_number != -1)
            {
                system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC " + program_name + ", " + std::to_string(duration_intr) + "\n";
                system_status += print_PCB(current, wait_queue);
                system_status += "\n";

                // run new program if trace file exists and has content
                if (!exec_traces.empty())
                {
                    auto [exec_exec, exec_status, exec_time] = simulate_trace(
                        exec_traces,
                        current_time,
                        vectors,
                        delays,
                        external_files,
                        current,
                        wait_queue
                    );

                    execution += exec_exec;
                    system_status += exec_status;
                    current_time = exec_time;
                }
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)
            // break ensures we stop executing instruction that no longer exist

        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (Set to partition 6)
    PCB current(0, -1, "init", 1, 6);
    
    memory[5].code = "init";

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/

    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
