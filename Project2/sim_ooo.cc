#include "sim_ooo.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>

using namespace std;

//used for debugging purposes
static const char *stage_names[NUM_STAGES] = {"ISSUE", "EXE", "WR", "COMMIT"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "XORI", "OR", "ORI", "AND", "ANDI", "MULT", "DIV", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "LWS", "SWS", "ADDS", "SUBS", "MULTS", "DIVS"};
static const char *res_station_names[5]={"Int", "Add", "Mult", "Load"};

/* convert a float into an unsigned */
inline unsigned float2unsigned(float value){
        unsigned result;
        memcpy(&result, &value, sizeof value);
        return result;
}

/* convert an unsigned into a float */
inline float unsigned2float(unsigned value){
        float result;
        memcpy(&result, &value, sizeof value);
        return result;
}

/* convert integer into array of unsigned char - little indian */
inline void unsigned2char(unsigned value, unsigned char *buffer){
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
}

/* convert array of char into integer - little indian */
inline unsigned char2unsigned(unsigned char *buffer){
       return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

sim_ooo::sim_ooo(unsigned mem_size,
                unsigned rob_size,
                unsigned num_int_res_stations,
                unsigned num_add_res_stations,
                unsigned num_mul_res_stations,
                unsigned num_load_res_stations,
		unsigned max_issue){
	//memory
	data_memory_size = mem_size;
	data_memory = new unsigned char[data_memory_size];
	
	//fill here
}
	
sim_ooo::~sim_ooo(){
}

void sim_ooo::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances){
}

void sim_ooo::load_program(const char *filename, unsigned base_address){
}

void sim_ooo::run(unsigned cycles){
}

//reset the state of the sim_oooulator
void sim_ooo::reset(){
}

int sim_ooo::get_int_register(unsigned reg){
	return UNDEFINED; //fill here
}

void sim_ooo::set_int_register(unsigned reg, int value){
}

float sim_ooo::get_fp_register(unsigned reg){
	return UNDEFINED; //fill here
}

void sim_ooo::set_fp_register(unsigned reg, float value){
}

unsigned sim_ooo::get_pending_int_register(unsigned reg){
	return UNDEFINED; //fill here
}

unsigned sim_ooo::get_pending_fp_register(unsigned reg){
	return UNDEFINED; //fill here
}

void sim_ooo::print_status(){
	print_pending_instructions();
	print_rob();
	print_reservation_stations();
	print_registers();
}

void sim_ooo::print_memory(unsigned start_address, unsigned end_address){
	cout << "DATA MEMORY[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": "; 
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3){
			cout << endl;
		}
	} 
}

void sim_ooo::write_memory(unsigned address, unsigned value)
{
	unsigned2char(value,data_memory+address);
}

void sim_ooo::print_registers(){
        unsigned i;
	cout << "GENERAL PURPOSE REGISTERS" << endl;
	cout << setfill(' ') << setw(8) << "Register" << setw(22) << "Value" << setw(5) << "ROB" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++){
                if (get_pending_int_register(i)!=UNDEFINED) 
			cout << setfill(' ') << setw(7) << "R" << dec << i << setw(22) << "-" << setw(5) << get_pending_int_register(i) << endl;
                else if (get_int_register(i)!=(int)UNDEFINED) 
			cout << setfill(' ') << setw(7) << "R" << dec << i << setw(11) << get_int_register(i) << hex << "/0x" << setw(8) << setfill('0') << get_int_register(i) << setfill(' ') << setw(5) << "-" << endl;
        }
	for (i=0; i< NUM_GP_REGISTERS; i++){
                if (get_pending_fp_register(i)!=UNDEFINED) 
			cout << setfill(' ') << setw(7) << "F" << dec << i << setw(22) << "-" << setw(5) << get_pending_fp_register(i) << endl;
                else if (get_fp_register(i)!=UNDEFINED) 
			cout << setfill(' ') << setw(7) << "F" << dec << i << setw(11) << get_fp_register(i) << hex << "/0x" << setw(8) << setfill('0') << float2unsigned(get_fp_register(i)) << setfill(' ') << setw(5) << "-" << endl;
	}
	cout << endl;
}

void sim_ooo::print_rob(){
	cout << "REORDER BUFFER" << endl; 
	cout << setfill(' ') << setw(5) << "Entry" << setw(6) << "Busy" << setw(7) << "Ready" << setw(12) << "PC" << setw(10) << "State" << setw(6) << "Dest" << setw(12) << "Value" << endl;
	
	//fill here
	
	cout << endl;
}

void sim_ooo::print_reservation_stations(){
	cout << "RESERVATION STATIONS" << endl;
	cout  << setfill(' ');
	cout << setw(7) << "Name" << setw(6) << "Busy" << setw(12) << "PC" << setw(12) << "Vj" << setw(12) << "Vk" << setw(6) << "Qj" << setw(6) << "Qk" << setw(6) << "Dest" << setw(12) << "Address" << endl; 
	
	// fill here
	
	cout << endl;
}

void sim_ooo::print_pending_instructions(){
	cout << "PENDING INSTRUCTIONS STATUS" << endl;
	cout << setfill(' ');
	cout << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7) << "WR" << setw(7) << "Commit";
	cout << endl;
}

void sim_ooo::print_log(){
}

float sim_ooo::get_IPC(){
	return UNDEFINED; //fill here
}
	
unsigned sim_ooo::get_instructions_executed(){
	return UNDEFINED; //fill here
}

unsigned sim_ooo::get_clock_cycles(){
	return UNDEFINED; //fill here
}

unsigned sim_ooo::get_register_value(std::string str)
{
	unsigned rIndex = str.find_first_of('R');
	string reg = str.substr(rIndex + 1);
	int value = strtoul(reg.c_str(), NULL, 10);
	return value;
}

unsigned sim_ooo::get_first_letter(std::string str, unsigned start)

{
	unsigned index = start;
	unsigned i;
	for (i = start; i < str.length(); i++)
	{
		if (str[i] != '	' && str[i] != ' ')
		{
			index = i;
			break;
		}
	}
	return index;
}

unsigned sim_ooo::find_end_of_argument(std::string str, unsigned start)
{
	unsigned index = start;
	unsigned i;
	for (i = start; i < str.length(); i++)
	{
		if (str[i] == '	' || str[i] == ' ')
		{
			index = i;
			break;
		}
	}
	if (i == str.length())
	{
		index = str.length() + 1;
	}
	return index;
}

int sim_ooo::convert_string_to_number(std::string str)
{
	int imm = 0;
	if (str.find('b') < str.length())
	{
		str = str.substr(2);
		imm = strtoul(str.c_str(), NULL, 2);
	}
	else if ((str.find('x') < str.length()) || (str.find('X') < str.length()))
	{
		str = str.substr(2);
		imm = strtoul(str.c_str(), NULL, 16);
	}
	else if (str.find('d') < str.length())
	{
		str = str.substr(2);
		imm = strtoul(str.c_str(), NULL, 10);
	}
	else
	{
		str = str.substr(0);
		imm = strtoul(str.c_str(), NULL, 10);
	}
	return imm;
}

bool sim_ooo::branchIf(unsigned opcode, unsigned a)
{
	bool condition = false;
	if (opcode == BEQZ && a == 0)
	{
		condition = true;
	}
	else if (opcode == BNEZ && a != 0)
	{
		condition = true;
	}
	else if (opcode == BLTZ && a < 0)
	{
		condition = true;
	}
	else if (opcode == BGTZ && a > 0)
	{
		condition = true;
	}
	else if (opcode == BLEZ && a <= 0)
	{
		condition = true;
	}
	else if (opcode == BGEZ && a >= 0)
	{
		condition = true;
	}
	return condition;
}