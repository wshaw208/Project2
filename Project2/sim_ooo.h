#ifndef SIM_OO_H_
#define SIM_OO_H_

#include <stdio.h>
#include <stdbool.h>
#include <string>
#include <string.h>
#include <sstream>

using namespace std;

#define UNDEFINED 0xFFFFFFFF //constant used for initialization
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 28
#define NUM_STAGES 4
#define BTABLE 50 //size of table for recording branche labels

typedef enum {LW = 1, SW = 2, ADD = 3, ADDI = 4, SUB = 5, SUBI = 06, XOR = 7, XORI = 8, OR = 9, ORI = 10, AND = 11, ANDI = 12, MULT = 13, DIV = 14, BEQZ = 15, BNEZ = 16, BLTZ = 17, BGTZ = 18, BLEZ = 19, BGEZ = 20, JUMP = 21, EOP = 22, LWS = 23, SWS = 24, ADDS = 25, SUBS = 26, MULTS = 27, DIVS = 28} opcode_t;

typedef enum {INTEGER_RS = 1, ADD_RS = 2, MULT_RS = 3, LOAD_B = 4} res_station_t;

typedef enum {INTEGER = 1, ADDER = 2, MULTIPLIER = 3, DIVIDER = 4, MEMORY = 5} exe_unit_t;

typedef enum{ISSUE = 1, EXECUTE = 2, WRITE_RESULT = 3, COMMIT = 4} stage_t;

struct reservation_station;
struct ex_unit;
struct read_order_buffer;

class sim_ooo{

	/* Add the data members required by your simulator's implementation here */

	//data memory - should be initialize to all 0xFF
	unsigned char *data_memory;

	//memory size in bytes
	unsigned data_memory_size;
	unsigned instruction_memory_size;
	unsigned pc;
	unsigned base_Address;

	//instruction memory
	unsigned int *instruction_memory;

	unsigned r_reg[NUM_GP_REGISTERS];
	float f_reg[NUM_GP_REGISTERS];
	unsigned opcode[NUM_OPCODES];

	float clock_cycles;
	float instruction_count;
	float stalls;

	bool eop;
	bool stalled;

	unsigned issue_max;
	unsigned rob_entry;
public:

	/* Instantiates the simulator
          	Note: registers must be initialized to UNDEFINED value, and data memory to all 0xFF values
        */
	sim_ooo(unsigned mem_size, 		// size of data memory (in byte)
		unsigned rob_size, 		// number of ROB entries
                unsigned num_int_res_stations,	// number of integer reservation stations 
                unsigned num_add_res_stations,	// number of ADD reservation stations
                unsigned num_mul_res_stations, 	// number of MULT/DIV reservation stations
                unsigned num_load_buffers,	// number of LOAD buffers
		unsigned issue_width=1		// issue width
        );	
	
	//de-allocates the simulator
	~sim_ooo();

        // adds one or more execution units of a given type to the processor
        // - exec_unit: type of execution unit to be added
        // - latency: latency of the execution unit (in clock cycles)
        // - instances: number of execution units of this type to be added
        void init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances=1);

	//loads the assembly program in file "filename" in instruction memory at the specified address
	void load_program(const char *filename, unsigned base_address=0x0);

	//runs the simulator for "cycles" clock cycles (run the program to completion if cycles=0) 
	void run(unsigned cycles=0);
	
	//resets the state of the simulator
        /* Note: 
	   - registers should be reset to UNDEFINED value 
	   - data memory should be reset to all 0xFF values
	   - instruction window, reservation stations and rob should be cleaned
	*/
	void reset();

       //returns value of the specified integer general purpose register
        int get_int_register(unsigned reg);

        //set the value of the given integer general purpose register to "value"
        void set_int_register(unsigned reg, int value);

        //returns value of the specified floating point general purpose register
        float get_fp_register(unsigned reg);

        //set the value of the given floating point general purpose register to "value"
        void set_fp_register(unsigned reg, float value);

	// returns the index of the ROB entry that will write this integer register (UNDEFINED if the value of the register is not pending
	unsigned get_pending_int_register(unsigned reg);

	// returns the index of the ROB entry that will write this floating point register (UNDEFINED if the value of the register is not pending
	unsigned get_pending_fp_register(unsigned reg);

	//returns the IPC
	float get_IPC();

	//returns the number of instructions fully executed
	unsigned get_instructions_executed();

	//returns the number of clock cycles 
	unsigned get_clock_cycles();

	//prints the content of the data memory within the specified address range
	void print_memory(unsigned start_address, unsigned end_address);

	// writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness)
	void write_memory(unsigned address, unsigned value);

	//prints the values of the registers 
	void print_registers();

	//prints the status of processor excluding memory
	void print_status();

	// prints the content of the ROB
	void print_rob();

	//prints the content of the reservation stations
	void print_reservation_stations();

	//print the content of the instruction window
	void print_pending_instructions();

	//print the whole execution history 
	void print_log();

	//returns the decimal value/address of a register
	unsigned get_register_value(std::string str);

	//returns the index of the first letter/number in the string
	unsigned get_first_letter(std::string str, unsigned start);

	//returns the index of the next space or tab
	unsigned find_end_of_argument(std::string str, unsigned start);

	//returns the int value of a string
	int convert_string_to_number(std::string str);

	void print_instruction_memory(unsigned start_address, unsigned end_address);

	//returns true if opcode condition and matches the value of a
	//returns false if not
	bool branchIf(unsigned opcode, unsigned a);

	void issue();

	void execute();

	void write_result();

	void commit();

	int get_open_rs(reservation_station *rs);

	int get_open_rob(read_order_buffer *rob);

	void write_to_rob_issue(unsigned instruction, unsigned open_rob, unsigned entry, unsigned destination, bool int_or_float);

	void write_to_rs(unsigned open_rs, reservation_station *rs, unsigned opcode, bool int_or_float, int vj, int vk, float vjf, float vkf, unsigned qj, unsigned qk, unsigned dest, string a);

	unsigned get_q(unsigned i, bool int_or_float);

	string make_a(unsigned instruction, bool int_or_float);

	int get_vx(unsigned reg);

	float get_vxf(unsigned reg);

	int get_rob(read_order_buffer *rob, unsigned dest);

	bool station_ready(reservation_station rs);
};

#endif /*SIM_OOO_H_*/
