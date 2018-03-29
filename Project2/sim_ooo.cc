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
	
sim_ooo::~sim_ooo()
{
	delete(data_memory);
}

void sim_ooo::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances)
{

}

void sim_ooo::load_program(const char *filename, unsigned base_address)
{
	instruction_memory = new unsigned int[base_address + 100];
	for (unsigned i = 0; i<base_address + 100; i++)
	{
		instruction_memory[i] = 0x00;
	}

	string line;
	ifstream program(filename, ios::in | ios::binary);

	if (program.is_open())
	{
		unsigned i = base_address;
		base_Address = base_address;
		instruction_memory_size = base_address;

		string param, offset, address;
		string labelTable[BTABLE];
		unsigned addressTable[BTABLE];
		unsigned table_index = 0, index, reg;
		unsigned space1, space2, space3;

		//gets all the label in the program
		while (getline(program, line))
		{
			if (line.find(':') < line.length())
			{
				string label = line.substr(0, line.find(':'));
				labelTable[table_index] = label;
				addressTable[table_index] = i;
				table_index++;
			}
			i++;
		}

		//close and reopen asm file
		program.close();
		i = base_address;
		ifstream program(filename, ios::in | ios::binary);


		//gets assembley code and decodes into instruction memory
		while (getline(program, line))
		{
			//getline(program,line);
			index = 0;
			index = get_first_letter(line, index);
			space1 = find_end_of_argument(line, index);
			if (space1 == 0xFFFFFFFF) // end of file
			{
				instruction_memory[i] += EOP;
				instruction_memory[i] = instruction_memory[i] << 26;
				break;
			}
			string opcode = line.substr(index, space1 - index);
			if (line.find(':') < line.length())
			{
				index = get_first_letter(line, space1); // line.find_first_of('	', index + 1);
				space1 = find_end_of_argument(line, index);
				opcode = line.substr(index, space1 - index);
			}

			if (opcode == "LW" || opcode == "SW" || opcode == "LWS" || opcode == "SWS")
			{
				//load opcode
				if (opcode == "LW")
				{
					instruction_memory[i] += LW;
				}
				else if (opcode == "SW")
				{
					instruction_memory[i] += SW;
				}
				else if (opcode == "LWS")
				{
					instruction_memory[i] += LWS;
				}
				else if (opcode == "SWS")
				{
					instruction_memory[i] += SWS;
				}
				instruction_memory[i] = instruction_memory[i] << 5;
				//load rs
				space2 = get_first_letter(line, space1);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 16;
				//load offset
				space2 = get_first_letter(line, space3);
				space3 = line.find_first_of('(', space2);
				offset = line.substr(space2, space3 - space2);
				reg = strtoul(offset.c_str(), NULL, 10);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 5;
				//load base
				space2 = space3;
				space3 = line.find_first_of(')', space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
			}
			else if (opcode == "ADD" || opcode == "SUB" || opcode == "XOR" || opcode == "OR"
				|| opcode == "AND" || opcode == "MULT" || opcode == "DIV" || opcode == "ADDS"
				|| opcode == "SUBS" || opcode == "MULTS" || opcode == "DIVS")
			{
				//load opcode
				if (opcode == "ADD")
				{
					instruction_memory[i] += ADD;
				}
				else if (opcode == "SUB")
				{
					instruction_memory[i] += SUB;
				}
				else if (opcode == "XOR")
				{
					instruction_memory[i] += XOR;
				}
				else if (opcode == "OR")
				{
					instruction_memory[i] += OR;
				}
				else if (opcode == "AND")
				{
					instruction_memory[i] += AND;
				}
				else if (opcode == "MULT")
				{
					instruction_memory[i] += MULT;
				}
				else if (opcode == "DIV")
				{
					instruction_memory[i] += DIV;
				}
				else if (opcode == "ADDS")
				{
					instruction_memory[i] += ADDS;
				}
				else if (opcode == "SUBS")
				{
					instruction_memory[i] += SUBS;
				}
				else if (opcode == "MULTS")
				{
					instruction_memory[i] += MULTS;
				}
				else if (opcode == "DIVS")
				{
					instruction_memory[i] += DIVS;
				}
				instruction_memory[i] = instruction_memory[i] << 5;
				//load rs
				space2 = get_first_letter(line, space1);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 5;
				//load rt
				space2 = get_first_letter(line, space3);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 5;
				//load rd
				space2 = get_first_letter(line, space3);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 11;
			}
			else if (opcode == "ADDI" || opcode == "SUBI" || opcode == "XORI"
				|| opcode == "ORI" || opcode == "ANDI")
			{
				//load opcode
				if (opcode == "ADDI")
				{
					instruction_memory[i] += ADDI;
				}
				else if (opcode == "SUBI")
				{
					instruction_memory[i] += SUBI;
				}
				else if (opcode == "XORI")
				{
					instruction_memory[i] += XORI;
				}
				else if (opcode == "ORI")
				{
					instruction_memory[i] += ORI;
				}
				else if (opcode == "ANDI")
				{
					instruction_memory[i] += ANDI;
				}

				instruction_memory[i] = instruction_memory[i] << 5;
				//load rs
				space2 = get_first_letter(line, space1);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 5;
				//load rt
				space2 = get_first_letter(line, space3);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 16;
				//load immediate
				space2 = get_first_letter(line, space3);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				unsigned imm = convert_string_to_number(param);// convert string hex to int
				instruction_memory[i] += imm;
			}
			else if (opcode == "BEQZ" || opcode == "BNEZ" || opcode == "BLTZ"
				|| opcode == "BGTZ" || opcode == "BLEZ" || opcode == "BGEZ")
			{
				//load opcode
				if (opcode == "BEQZ")
				{
					instruction_memory[i] += BEQZ;
				}
				else if (opcode == "BNEZ")
				{
					instruction_memory[i] += BNEZ;
				}
				else if (opcode == "BLTZ")
				{
					instruction_memory[i] += BLTZ;
				}
				else if (opcode == "BGTZ")
				{
					instruction_memory[i] += BGTZ;
				}
				else if (opcode == "BLEZ")
				{
					instruction_memory[i] += BLEZ;
				}
				else if (opcode == "BGEZ")
				{
					instruction_memory[i] += BGEZ;
				}
				instruction_memory[i] = instruction_memory[i] << 5;
				//load rs
				space2 = get_first_letter(line, space1);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				reg = get_register_value(param);
				instruction_memory[i] += reg;
				instruction_memory[i] = instruction_memory[i] << 21;
				//get address
				space2 = get_first_letter(line, space3);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				for (int j = 0; j<BTABLE; j++)
				{
					if (param.compare(labelTable[j]) == 0)
					{
						unsigned reduced_address = addressTable[j] - base_Address;
						instruction_memory[i] += reduced_address;
						break;
					}
				}
			}
			else if (opcode == "JUMP")
			{
				instruction_memory[i] += JUMP;
				instruction_memory[i] = instruction_memory[i] << 26;

				space2 = get_first_letter(line, space1);
				space3 = find_end_of_argument(line, space2);
				param = line.substr(space2, space3 - space2);
				for (int j = 0; j<BTABLE; j++)
				{
					if (param.compare(labelTable[j]) == 0)
					{
						unsigned reduced_address = addressTable[j] - base_Address;
						instruction_memory[i] += reduced_address;
						break;
					}
				}
			}
			else if (opcode == "EOP")
			{
				//load opcode
				instruction_memory[i] += EOP;
				instruction_memory[i] = instruction_memory[i] << 26;
			}
			else
			{

			}

			unsigned answer = instruction_memory[i];
			answer = answer;
			i++;
		}
		pc = base_address;
		program.close();

	}
	else
	{
		instruction_count++;
		return;
	}
}

void sim_ooo::run(unsigned cycles)
{
	if (cycles == 0)
	{
		while (!eop)
		{
			
			clock_cycles++;
		}
	}
	else //run for select amount of cycles
	{
		unsigned i;
		for (i = 0; i<cycles; i++)
		{
			
			clock_cycles++;
		}
	}
}

//reset the state of the sim_oooulator
void sim_ooo::reset()
{
	//Add reset for reservation stations
	for (int i = 0; i < NUM_GP_REGISTERS; i++)
	{
		r_reg[i] = UNDEFINED;
		f_reg[i] = (float)UNDEFINED;
	}
	clock_cycles = 0;
	instruction_count = 0;
}

int sim_ooo::get_int_register(unsigned reg)
{
	return r_reg[reg];
}

void sim_ooo::set_int_register(unsigned reg, int value)
{
	r_reg[reg] = value;
}

float sim_ooo::get_fp_register(unsigned reg)
{
	return f_reg[reg];
}

void sim_ooo::set_fp_register(unsigned reg, float value)
{
	f_reg[reg] = value;
}

unsigned sim_ooo::get_pending_int_register(unsigned reg)
{
	return UNDEFINED; //fill here
}

unsigned sim_ooo::get_pending_fp_register(unsigned reg)
{
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

void sim_ooo::print_log()
{

}

float sim_ooo::get_IPC()
{
	return instruction_count / clock_cycles;
}
	
unsigned sim_ooo::get_instructions_executed()
{
	return (unsigned)instruction_count;
}

unsigned sim_ooo::get_clock_cycles()
{
	return (unsigned)clock_cycles;
}

unsigned sim_ooo::get_register_value(std::string str)
{
	unsigned index = 0;
	if (str.find('R') != std::string::npos) 
	{
		index = str.find_first_of('R');
	}
	else if (str.find('r') != std::string::npos)
	{
		index = str.find_first_of('r');
	}
	else if (str.find('F') != std::string::npos)
	{
		index = str.find_first_of('F');
	}
	else if (str.find('f') != std::string::npos)
	{
		index = str.find_first_of('f');
	}
	string reg = str.substr(index + 1);
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

void sim_ooo::print_instruction_memory(unsigned start_address, unsigned end_address) {
	cout << "instruction_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') << end_address << "]" << endl;
	unsigned i;
	for (i = instruction_memory_size; i<instruction_memory_size + 100; i++) {
		if (i % 1 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
		cout << hex << setw(2) << setfill('0') << int(instruction_memory[i]) << " ";
		if (i % 4 == 3) cout << endl;
	}
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