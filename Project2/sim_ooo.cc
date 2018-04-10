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

struct reservation_station
{
	string name;
	bool busy = false;
	unsigned opcode;
	int vj, vk;
	float vjf, vkf;
	unsigned qj, qk, dest;
	unsigned a;
	unsigned pc;
	bool wb = false;
};

struct ex_unit
{
	string name;
	bool busy = false;
	unsigned opcode;
	int vj, vk;
	float vjf, vkf;
	int ttf, delay; //ttf time-to-finish
	unsigned entry;
	unsigned pc;
};

struct read_order_buffer
{
	unsigned entry;
	bool busy, ready;
	unsigned instruction;
	string state;
	string destination;
	int value;
	float value_f;
	unsigned pc;
};

struct int_register
{
	int value;
	unsigned entry;
};

struct fp_register
{
	float value;
	unsigned entry;
};

struct instruction_q
{
	unsigned pc;
	unsigned Issue, Exe, WR, Commit;
};

reservation_station* int_rs;
reservation_station* add_rs;
reservation_station* mult_rs;
reservation_station* load_rs;

ex_unit* int_ex;
ex_unit* add_ex;
ex_unit* mult_ex;
ex_unit* div_ex;
ex_unit* mem_ex;

read_order_buffer* rob;

int_register* int_reg;
fp_register* fp_reg;

instruction_q* iq;

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
	for (unsigned i = 0; i < data_memory_size; i++)
	{
		data_memory[i] = 0xFF;
	}

	//fill here
	issue_max = max_issue;

	int_reg = new int_register[NUM_GP_REGISTERS];
	fp_reg = new fp_register[NUM_GP_REGISTERS];
	reset();

	int_rs = new reservation_station[num_int_res_stations];
	add_rs = new reservation_station[num_add_res_stations];
	mult_rs = new reservation_station[num_mul_res_stations];
	load_rs = new reservation_station[num_load_res_stations];

	size_of_int_rs = num_int_res_stations;
	size_of_add_rs = num_add_res_stations;
	size_of_mult_rs = num_mul_res_stations;
	size_of_load_rs = num_load_res_stations;

	for (unsigned i = 0; i < num_int_res_stations; i++)
	{
		int_rs[i].name = "Int" + to_string(i+1);
		int_rs[i].busy = false;
	}
	for (unsigned i = 0; i < num_add_res_stations; i++)
	{
		add_rs[i].name = "Add" + to_string(i+1);
		add_rs[i].busy = false;
	}
	for (unsigned i = 0; i < num_mul_res_stations; i++)
	{
		mult_rs[i].name = "Mult" + to_string(i+1);
		mult_rs[i].busy = false;
	}
	for (unsigned i = 0; i < num_load_res_stations; i++)
	{
		load_rs[i].name = "Load" + to_string(i+1);
		load_rs[i].busy = false;
	}

	rob = new read_order_buffer[rob_size];
	size_of_rob = rob_size;
	iq = new instruction_q[size_of_rob];
	flush_rob();
}
	
sim_ooo::~sim_ooo()
{
	delete(data_memory);
}

void sim_ooo::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances)
{
	string unit_name;
	
	switch (exec_unit)
	{
	case INTEGER:
		unit_name = "INT";
		int_ex = new ex_unit[instances];
		size_of_int_ex = instances;
		break;
	case ADDER:
		unit_name = "ADD";
		add_ex = new ex_unit[instances];
		size_of_add_ex = instances;
		break;
	case MULTIPLIER:
		unit_name = "MULT";
		mult_ex = new ex_unit[instances];
		size_of_mult_ex = instances;
		break;
	case DIVIDER:
		unit_name = "DIV";
		div_ex = new ex_unit[instances];
		size_of_div_ex = instances;
		break;
	case MEMORY:
		unit_name = "MEM";
		mem_ex = new ex_unit[instances];
		size_of_mem_ex = instances;
		break;
	}

	for(unsigned i = 0; i < instances; i++)
	{
		switch (exec_unit)
		{
		case INTEGER:
			int_ex[i].name = unit_name + to_string(i+1);
			int_ex[i].delay = latency+1;
			break;
		case ADDER:
			add_ex[i].name = unit_name + to_string(i+1);
			add_ex[i].delay = latency+1;
			break;
		case MULTIPLIER:
			mult_ex[i].name = unit_name + to_string(i+1);
			mult_ex[i].delay = latency+1;
			break;
		case DIVIDER:
			div_ex[i].name = unit_name + to_string(i+1);
			div_ex[i].delay = latency+1;
			break;
		case MEMORY:
			mem_ex[i].name = unit_name + to_string(i+1);
			mem_ex[i].delay = latency+1;
			break;
		}
	}

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
			commit();
			write_result();
			execute();
			issue();
			clock_cycles++;
		}
	}
	else //run for select amount of cycles
	{
		unsigned i;
		for (i = 0; i<cycles; i++)
		{
			commit();
			write_result();
			execute();
			issue();
			clock_cycles++;
			if (eop)
			{
				break;
			}
		}
	}
}

//reset the state of the sim_oooulator
void sim_ooo::reset()
{
	//Add reset for reservation stations
	for (int i = 0; i < NUM_GP_REGISTERS; i++)
	{
		int_reg[i].value = UNDEFINED;
		int_reg[i].entry = UNDEFINED;
		fp_reg[i].value = (float)(UNDEFINED);
		fp_reg[i].entry = UNDEFINED;
	}
	clock_cycles = 0;
	instruction_count = 0;
}

int sim_ooo::get_int_register(unsigned reg)
{
	unsigned rob_entry = get_pending_int_register(reg);
	if (rob_entry != UNDEFINED)
	{
		if (rob[rob_entry].ready)
		{
			return rob[rob_entry].value;
		}
		else
		{
			return UNDEFINED;
		}
	}
	return int_reg[reg].value;
}

void sim_ooo::set_int_register(unsigned reg, int value)
{
	int_reg[reg].value = value;
}

float sim_ooo::get_fp_register(unsigned reg)
{
	unsigned rob_entry = get_pending_fp_register(reg);
	if (rob_entry != UNDEFINED)
	{
		if (rob[rob_entry].ready)
		{
			return rob[rob_entry].value_f;
		}
		else
		{
			return unsigned2float(UNDEFINED);
		}
	}
	
	return fp_reg[reg].value;
}

void sim_ooo::set_fp_register(unsigned reg, float value)
{
	fp_reg[reg].value = value;
}

unsigned sim_ooo::get_pending_int_register(unsigned reg)
{
	unsigned index = UNDEFINED;
	unsigned size = size_of_rob;
	for (unsigned i = 0; i < size; i++)
	{
		if (rob[i].destination == ("R" + to_string(reg)))
		{
			index = i;
		}
	}
	return index;
}

unsigned sim_ooo::get_pending_fp_register(unsigned reg)
{
	unsigned index = UNDEFINED;
	unsigned size = size_of_rob;
	for (unsigned i = 0; i < size; i++)
	{
		if (rob[i].destination == ("F" + to_string(reg)))
		{
			index = i;
		}
	}
	return index;
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
	unsigned size = size_of_rob;
	string busy, ready, state, dest;
	bool good_pc, good_value;
	for (unsigned i = 0; i < size; i++)
	{		
		if (rob[i].busy)
		{
			busy = "yes";
		}
		else
		{
			busy = "no";
		}
		if (rob[i].ready)
		{
			ready = "yes";
		}
		else
		{
			ready = "no";
		}
		if (rob[i].pc != UNDEFINED)
		{
			good_pc = true;
		}
		else
		{
			good_pc = false;
		}
		if (rob[i].state != "")
		{
			state = rob[i].state;
		}
		else
		{
			state = "-";
		}
		if (rob[i].destination != "")
		{
			dest = rob[i].destination;
		}
		else
		{
			dest = "-";
		}
		if (rob[i].value != UNDEFINED)
		{
			good_value = true;
		}
		else
		{
			good_value = false;
		}

		cout << setfill(' ') << setw(5) << to_string(i+1) << setw(6) << busy << setw(7) << ready;
		if (good_pc)
		{
			cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << rob[i].pc;
		}
		else
		{
			cout << setw(12) << "-" << setw(8);
		}
		cout << setfill(' ') << setw(10) << state << setw(6);
		unsigned opcode = (rob[i].instruction >> 26) & 31;
		if (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ
			|| opcode == BGTZ || opcode == BLEZ || opcode == BGEZ
			|| opcode == JUMP)
		{
			cout << "-";
		}
		else
		{
			cout << dest;
		}
		if (good_value)
		{
			cout << setw(4) << hex << "0x" << setw(2) << setfill('0') << rob[i].value;
		}
		else
		{
			cout << setw(12) << "-" << setw(8);
		}
		cout << endl;
	}
	cout << endl;
}

void sim_ooo::print_reservation_stations(){
	cout << "RESERVATION STATIONS" << endl;
	cout  << setfill(' ');
	cout << setw(7) << "Name" << setw(6) << "Busy" << setw(12) << "PC" << setw(12) << "Vj" << setw(12) << "Vk" << setw(6) << "Qj" << setw(6) << "Qk" << setw(6) << "Dest" << setw(12) << "Address" << endl; 
	
	// fill here
	// print out int_rs
	for (unsigned i = 0; i < size_of_int_rs; i++)
	{
		for (unsigned j = 0; j < 9; j++)
		{
			switch (j)
			{
			case 0:
				cout << setw(7) << int_rs[i].name << setw(6);
				break;
			case 1:
				if (int_rs[i].busy)
				{
					cout << "yes";
				}
				else
				{
					cout << "no";
				}
				break;
			case 2:
				if (int_rs[i].pc < 10000000)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << int_rs[i].pc << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 3:
				if (int_rs[i].vj < 65536 && int_rs[i].vj > -65536 && int_rs[i].vj != UNDEFINED)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << int_rs[i].vj << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 4:
				if (int_rs[i].vk < 65536 && int_rs[i].vk > -65536 && int_rs[i].vk != UNDEFINED)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << int_rs[i].vk << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 5:
				if (int_rs[i].qj < 32)
				{
					cout << setw(6) << to_string(int_rs[i].qj);
				}
				else
				{
					cout << setw(6) << "-" ;
				}
				break;
			case 6:
				if (int_rs[i].qk < 32)
				{
					cout << setw(6) << to_string(int_rs[i].qk);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 7:
				if (int_rs[i].dest < size_of_rob)
				{
					cout << setw(6) << to_string(int_rs[i].dest);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 8:
				cout << setw(12) << "-" << setw(8) << endl;
				break;
			}
		}
	}
	// print out Load_rs
	for (unsigned i = 0; i < size_of_load_rs; i++)
	{
		for (unsigned j = 0; j < 9; j++)
		{
			switch (j)
			{
			case 0:
				cout << setw(7) << load_rs[i].name << setw(6);
				break;
			case 1:
				if (load_rs[i].busy)
				{
					cout << "yes";
				}
				else
				{
					cout << "no";
				}
				break;
			case 2:
				if (load_rs[i].pc < 10000000)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << load_rs[i].pc << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 3:
				if (load_rs[i].vj < 100000 && load_rs[i].vj > -100000 && load_rs[i].vj != UNDEFINED)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << load_rs[i].vj << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 4:
				cout << setw(12) << "-" << setw(8);
				break;
			case 5:
				if (load_rs[i].qj < 32)
				{
					cout << setw(6) << to_string(load_rs[i].qj);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 6:
				if (load_rs[i].qk < 32)
				{
					cout << setw(6) << to_string(load_rs[i].qk);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 7:
				if (load_rs[i].dest < size_of_rob)
				{
					cout << setw(6) << to_string(load_rs[i].dest);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 8:
				if (load_rs[i].dest < size_of_rob)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << load_rs[i].a << setfill(' ') << endl;
				}
				else
				{
					cout << setw(12) << "-" << setw(8) << endl;
				}
				break;
			}
		}
	}
	// print out add_rs
	for (unsigned i = 0; i < size_of_add_rs; i++)
	{
		for (unsigned j = 0; j < 9; j++)
		{
			switch (j)
			{
			case 0:
				cout << setw(7) << add_rs[i].name << setw(6);
				break;
			case 1:
				if (add_rs[i].busy)
				{
					cout << "yes";
				}
				else
				{
					cout << "no";
				}
				break;
			case 2:
				if (add_rs[i].pc < 10000000)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << add_rs[i].pc << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 3:
				if (add_rs[i].opcode == ADDS || add_rs[i].opcode == SUBS)
				{
					if (add_rs[i].vjf < 100000.00 && add_rs[i].vjf > -100000.00 && add_rs[i].vjf != unsigned2float(UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << float2unsigned(add_rs[i].vjf) << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				else
				{
					if (add_rs[i].vj < 100000 && add_rs[i].vj > -100000 && add_rs[i].vj != (UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << add_rs[i].vj << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				break;
			case 4:
				if (add_rs[i].opcode == ADDS || add_rs[i].opcode == SUBS)
				{
					if (add_rs[i].vkf < 100000.00 && add_rs[i].vkf > -100000.00 && add_rs[i].vkf != unsigned2float(UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << float2unsigned(add_rs[i].vkf) << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				else
				{
					if (add_rs[i].vk < 100000 && add_rs[i].vk > -100000 && add_rs[i].vk != (UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << add_rs[i].vk << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				break;
			case 5:
				if (add_rs[i].qj < 32)
				{
					cout << setw(6) << to_string(add_rs[i].qj);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 6:
				if (add_rs[i].qk < 32)
				{
					cout << setw(6) << to_string(add_rs[i].qk);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 7:
				if (add_rs[i].dest < size_of_rob)
				{
					cout << setw(6) << to_string(add_rs[i].dest);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 8:
				cout << setw(12) << "-" << setw(8) << endl;
				break;
			}
		}
	}
	// print out mult_rs
	for (unsigned i = 0; i < size_of_mult_rs; i++)
	{
		for (unsigned j = 0; j < 9; j++)
		{
			switch (j)
			{
			case 0:
				cout << setw(7) << mult_rs[i].name << setw(6);
				break;
			case 1:
				if (mult_rs[i].busy)
				{
					cout << "yes";
				}
				else
				{
					cout << "no";
				}
				break;
			case 2:
				if (mult_rs[i].pc < 10000000)
				{
					cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << mult_rs[i].pc << setfill(' ');
				}
				else
				{
					cout << setw(12) << "-" << setw(8);
				}
				break;
			case 3:
				if (mult_rs[i].opcode == MULTS || mult_rs[i].opcode == DIVS)
				{
					if (mult_rs[i].vjf < 100000.00 && mult_rs[i].vjf > -100000.00 && mult_rs[i].vjf != unsigned2float(UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << float2unsigned(mult_rs[i].vjf) << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				else
				{
					if (mult_rs[i].vj < 100000 && mult_rs[i].vj > -100000 && mult_rs[i].vj != (UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << mult_rs[i].vj << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				break;
			case 4:
				if (mult_rs[i].opcode == MULTS || mult_rs[i].opcode == DIVS)
				{
					if (mult_rs[i].vkf < 100000.00 && mult_rs[i].vkf > -100000.00 && mult_rs[i].vkf != unsigned2float(UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << float2unsigned(mult_rs[i].vkf) << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				else
				{
					if (mult_rs[i].vk < 100000 && mult_rs[i].vk > -100000 && mult_rs[i].vk != (UNDEFINED))
					{
						cout << setw(4) << hex << "0x" << setw(8) << setfill('0') << mult_rs[i].vk << setfill(' ');
					}
					else
					{
						cout << setw(12) << "-" << setw(8);
					}
				}
				break;
			case 5:
				if (mult_rs[i].qj < 32)
				{
					cout << setw(6) << to_string(mult_rs[i].qj);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 6:
				if (mult_rs[i].qk < 32)
				{
					cout << setw(6) << to_string(mult_rs[i].qk);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 7:
				if (mult_rs[i].dest < size_of_rob)
				{
					cout << setw(6) << to_string(mult_rs[i].dest);
				}
				else
				{
					cout << setw(6) << "-";
				}
				break;
			case 8:
				cout << setw(12) << "-" << setw(8) << endl;
				break;
			}
		}
	}
	cout << endl;
}

void sim_ooo::print_pending_instructions(){
	cout << "PENDING INSTRUCTIONS STATUS" << endl;
	cout << setfill(' ');
	cout << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7) << "WR" << setw(7) << "Commit";
	cout << endl;

	for (unsigned i = 0; i < size_of_rob; i++)
	{
		for (unsigned j = 0; j < 5; j++)
		{
			switch (j)
			{
			case 0:
				if (iq[i].pc != UNDEFINED)
				{
					cout << setw(2) << hex << "0x" << setw(8) << setfill('0') << iq[i].pc << setfill(' ');
				}
				else
				{
					cout << setw(10) << "-";
				}
				break;
			case 1:
				if (iq[i].Issue != UNDEFINED)
				{
					cout << setw(7) << to_string(iq[i].Issue);
				}
				else
				{
					cout << setw(7) << "-";
				}
				break;
			case 2:
				if (iq[i].Exe != UNDEFINED)
				{
					cout << setw(7) << to_string(iq[i].Exe);
				}
				else
				{
					cout << setw(7) << "-";
				}
				break;
			case 3:
				if (iq[i].WR != UNDEFINED)
				{
					cout << setw(7) << to_string(iq[i].WR);
				}
				else
				{
					cout << setw(7) << "-";
				}
				break;
			case 4:
				if (iq[i].Commit != UNDEFINED)
				{
					cout << setw(7) << to_string(iq[i].Commit) << endl;
				}
				else
				{
					cout << setw(7) << "-" << endl;
				}
				break;
			}
		}
	}
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

void sim_ooo::issue()
{
	for (unsigned i = 0; i < issue_max; i++)
	{
		bool int_or_float = true;
		int open_rob = get_open_rob(rob);
		unsigned destination, pc_entry = pc * 4;
		if (open_rob == -1) // if no open re-order buffer we stall the issue stage
		{
			return;
		}
		int opcode = (instruction_memory[pc] >> 26) & 31;
		if (opcode == LW || opcode == SW || opcode == SWS || opcode == LWS)
		{
			int open_rs = get_open_rs(load_rs);
			if (open_rs == -1) // if no open reservation station we stall the issue stage
			{
				return;
			}
			if (opcode == SWS || opcode == LWS)
			{
				int_or_float = false;
			}
			destination = ((instruction_memory[pc] >> 21) & 31);
			unsigned vj = get_int_register(instruction_memory[pc] & 31);
			float vjf = unsigned2float(UNDEFINED);
			unsigned vk = UNDEFINED;
			float vkf = unsigned2float(UNDEFINED);
			unsigned qj = get_q(instruction_memory[pc] & 31, int_or_float);
			unsigned qk = UNDEFINED;
			unsigned a = (instruction_memory[pc] >> 5) & 65535;
			write_to_rob_issue(instruction_memory[pc], open_rob, pc_entry, destination, int_or_float); // writes the instruction to the rob
			write_to_rs(open_rs, 4, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
		}

		else if (opcode == ADD || opcode == SUB || opcode == XOR || opcode == OR
			|| opcode == AND || opcode == MULT || opcode == DIV || opcode == ADDS
			|| opcode == SUBS || opcode == MULTS || opcode == DIVS)
		{
			if (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS)
			{
				int_or_float = false;
			}
			destination = ((instruction_memory[pc] >> 21) & 31);
			unsigned vj = get_int_register((instruction_memory[pc] >> 16) & 31);
			float vjf = get_fp_register((instruction_memory[pc] >> 16) & 31);
			unsigned vk = get_int_register((instruction_memory[pc] >> 11) & 31);
			float vkf = get_fp_register((instruction_memory[pc] >> 11) & 31);
			unsigned qj = get_q((instruction_memory[pc] >> 16) & 31, int_or_float);
			unsigned qk = get_q((instruction_memory[pc] >> 11) & 31, int_or_float);
			unsigned a = UNDEFINED;

			if (opcode == ADD || opcode == SUB || opcode == XOR || opcode == AND || opcode == OR)
			{
				int open_rs = get_open_rs(int_rs);
				if (open_rs == -1) // if no open reservation station we stall the issue stage
				{
					return;
				}
				write_to_rs(open_rs, 1, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
			}
			else if (opcode == ADDS || opcode == SUBS)
			{
				int open_rs = get_open_rs(add_rs);
				if (open_rs == -1) // if no open reservation station we stall the issue stage
				{
					return;
				}
				write_to_rs(open_rs, 2, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
			}
			else if (opcode == MULT || opcode == MULTS || opcode == DIV || opcode == DIVS)
			{
				int open_rs = get_open_rs(mult_rs);
				if (open_rs == -1) // if no open reservation station we stall the issue stage
				{
					return;
				}
				write_to_rs(open_rs, 3, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
			}

			write_to_rob_issue(instruction_memory[pc], open_rob, pc_entry, destination, int_or_float); // writes the instruction to the rob
		}
		else if (opcode == ADDI || opcode == SUBI || opcode == XORI
			|| opcode == ORI || opcode == ANDI)
		{
			destination = ((instruction_memory[pc] >> 21) & 31);
			unsigned vj = get_int_register((instruction_memory[pc] >> 16) & 31);
			float vjf = unsigned2float(UNDEFINED);
			unsigned vk = (instruction_memory[pc] & 65535);
			float vkf = unsigned2float(UNDEFINED);
			unsigned qj = get_q((instruction_memory[pc] >> 16) & 31, int_or_float);
			unsigned qk = UNDEFINED;
			unsigned a = UNDEFINED;
			int open_rs = get_open_rs(int_rs);
			if (open_rs == -1) // if no open reservation station we stall the issue stage
			{
				return;
			}
			write_to_rob_issue(instruction_memory[pc], open_rob, pc_entry, destination, int_or_float); // writes the instruction to the rob
			write_to_rs(open_rs, 1, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
		}
		else if (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ
			|| opcode == BGTZ || opcode == BLEZ || opcode == BGEZ)
		{
			destination = UNDEFINED;
			unsigned vj = get_int_register((instruction_memory[pc] >> 21) & 31);
			float vjf = unsigned2float(UNDEFINED);
			unsigned vk = UNDEFINED;
			float vkf = unsigned2float(UNDEFINED);
			unsigned qj = get_q((instruction_memory[pc] >> 21) & 31, int_or_float);
			unsigned qk = UNDEFINED;
			unsigned a = (instruction_memory[pc] & 65535) + base_Address;
			int open_rs = get_open_rs(int_rs);
			if (open_rs == -1) // if no open reservation station we stall the issue stage
			{
				return;
			}
			write_to_rob_issue(instruction_memory[pc], open_rob, pc_entry, destination, int_or_float); // writes the instruction to the rob
			write_to_rs(open_rs, 1, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
		}
		else if (opcode == JUMP)
		{
			destination = UNDEFINED;
			unsigned qj = UNDEFINED;
			unsigned qk = UNDEFINED;
			unsigned vj = UNDEFINED;
			float vjf = unsigned2float(UNDEFINED);
			unsigned vk = UNDEFINED;
			float vkf = unsigned2float(UNDEFINED);
			unsigned a = (instruction_memory[pc] & 65535) + base_Address;
			int open_rs = get_open_rs(int_rs);
			if (open_rs == -1) // if no open reservation station we stall the issue stage
			{
				return;
			}
			write_to_rob_issue(instruction_memory[pc], open_rob, pc_entry, destination, int_or_float); // writes the instruction to the rob
			write_to_rs(open_rs, 1, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
		}
		else if (opcode == EOP)
		{
			destination = UNDEFINED;
			unsigned qj = UNDEFINED;
			unsigned qk = UNDEFINED;
			unsigned vj = UNDEFINED;
			float vjf = unsigned2float(UNDEFINED);
			unsigned vk = UNDEFINED;
			float vkf = unsigned2float(UNDEFINED);
			unsigned a = UNDEFINED;
			int open_rs = get_open_rs(int_rs);
			if (open_rs == -1) // if no open reservation station we stall the issue stage
			{
				return;
			}
			write_to_rob_issue(instruction_memory[pc], open_rob, pc_entry, destination, int_or_float); // writes the instruction to the rob
			write_to_rs(open_rs, 1, opcode, int_or_float, vj, vk, vjf, vkf, qj, qk, pc_entry, a, open_rob);
		}
		pc++;
	}
}

void sim_ooo::execute()
{
	// check to see if any open exe units
	int i;
	bool ex_open;
	// int unit
	ex_open = false;
	int size = size_of_int_ex;
	for (i = 0; i < size; i++)
	{
		if (!int_ex[i].busy)
		{
			ex_open = true;
			break; // we have an open ex_unit so end loop
		} 
	}

	// check reservation station to see if the instructions are ready
	if (ex_open)
	{
		size = size_of_int_rs;
		for (int j = 0; j < size; j++)
		{
			unsigned rob_entry = int_rs[j].dest;// gets the rob that the instruction is held in
			if (rob_entry >= 0 && rob_entry <= size_of_rob)
			{
				if (rob[rob_entry].state == "ISSUE")
				{

					if (station_ready(int_rs[j]))//checks if we have all values necessary to compute
					{
						//if everything checks out then we move the instruction in the exe unit
						rob[rob_entry].state = "EXE";
						iq[rob_entry].Exe = clock_cycles;
						int_ex[i].busy = true;
						int_ex[i].ttf = int_ex[i].delay;
						int_ex[i].entry = int_rs[j].dest;
						int_ex[i].opcode = int_rs[j].opcode;
						int_ex[i].vj = int_rs[j].vj;
						if (int_rs[j].opcode == BEQZ || int_rs[j].opcode == BNEZ || int_rs[j].opcode == BLTZ
							|| int_rs[j].opcode == BGTZ || int_rs[j].opcode == BLEZ || int_rs[j].opcode == BGEZ
							|| int_rs[j].opcode == JUMP) // if a branch instruction write address to vk for computation
						{
							int_ex[j].vk = int_rs[j].a;
						}
						else
						{
							int_ex[i].vk = int_rs[j].vk;
						}
						int_ex[i].vjf = int_rs[j].vjf;
						int_ex[i].vkf = int_rs[j].vkf;
						int_ex[i].pc = int_rs[j].pc;
						break;
					}
				}
			}
		}
	}
	//add unit
	ex_open = false;
	size = size_of_add_ex;
	for (i = 0; i < size; i++)
	{
		if (!add_ex[i].busy)
		{
			ex_open = true;
			break; // we have an open ex_unit so end loop
		}
	}

	// check reservation station to see if the instructions are ready
	if (ex_open)
	{
		size = size_of_add_rs;
		for (int j = 0; j < size; j++)
		{
			unsigned rob_entry = add_rs[j].dest;// gets the rob that the instruction is held in
			if (rob_entry >= 0 && rob_entry <= size_of_rob)
			{
				if (rob[rob_entry].state == "ISSUE")
				{

					if (station_ready(add_rs[j]))//checks if we have all values necessary to compute
					{
						//if everything checks out then we move the instruction in the exe unit
						rob[rob_entry].state = "EXE";
						iq[rob_entry].Exe = clock_cycles;
						add_ex[i].busy = true;
						add_ex[i].ttf = add_ex[i].delay;
						add_ex[i].entry = add_rs[j].dest;
						add_ex[i].opcode = add_rs[j].opcode;
						add_ex[i].vj = add_rs[j].vj;
						add_ex[i].vk = add_rs[j].vk;
						add_ex[i].vjf = add_rs[j].vjf;
						add_ex[i].vkf = add_rs[j].vkf;
						add_ex[i].pc = add_rs[j].pc;
						break;
					}
				}
			}
		}
	}
	//mem unit
	ex_open = false;
	size = size_of_mem_ex;
	for (i = 0; i < size; i++)
	{
		if (!mem_ex[i].busy)
		{
			ex_open = true;
			break; // we have an open ex_unit so end loop
		}
	}

	// check reservation station to see if the instructions are ready
	if (ex_open)
	{
		size = size_of_load_rs;
		for (int j = 0; j < size; j++)
		{
			unsigned rob_entry = load_rs[j].dest;// gets the rob that the instruction is held in
			if (rob_entry >= 0 && rob_entry <= size_of_rob)
			{
				if (rob[rob_entry].state == "ISSUE")
				{

					if (station_ready(add_rs[j]))//checks if we have all values necessary to compute
					{
						//if everything checks out then we move the instruction in the exe unit
						rob[rob_entry].state = "EXE";
						iq[rob_entry].Exe = clock_cycles;
						mem_ex[i].busy = true;
						mem_ex[i].ttf = mem_ex[i].delay;
						mem_ex[i].entry = load_rs[j].dest;
						mem_ex[i].opcode = load_rs[j].opcode;

						mem_ex[i].vj = load_rs[j].vj;
						mem_ex[i].vk = load_rs[j].a;
						mem_ex[i].pc = load_rs[j].pc;
						break;
					}
				}
			}
		}
	}
	//mult unit
	ex_open = false;
	size = size_of_mult_ex;
	for (i = 0; i < size; i++)
	{
		if (!mult_ex[i].busy)
		{
			ex_open = true;
			break; // we have an open ex_unit so end loop
		}
	}

	// check reservation station to see if the instructions are ready
	if (ex_open)
	{
		size = size_of_mult_rs;
		for (int j = 0; j < size; j++)
		{
			unsigned rob_entry = mult_rs[j].dest;// gets the rob that the instruction is held in
			if (rob_entry >= 0 && rob_entry <= size_of_rob)
			{
				if (rob[rob_entry].state == "ISSUE")
				{

					if (station_ready(mult_rs[j]))//checks if we have all values necessary to compute
					{
						if (mult_rs[j].opcode == MULT || mult_rs[j].opcode == MULTS)
						{
							//if everything checks out then we move the instruction in the exe unit
							rob[rob_entry].state = "EXE";
							iq[rob_entry].Exe = clock_cycles;
							mult_ex[i].busy = true;
							mult_ex[i].ttf = mult_ex[i].delay;
							mult_ex[i].entry = mult_rs[j].dest;
							mult_ex[i].opcode = mult_rs[j].opcode;
							mult_ex[i].vj = mult_rs[j].vj;
							mult_ex[i].vk = mult_rs[j].vk;
							mult_ex[i].vjf = mult_rs[j].vjf;
							mult_ex[i].vkf = mult_rs[j].vkf;
							mult_ex[i].pc = mult_rs[j].pc;
							break;
						}
					}
				}
			}
		}
	}
	//div unit
	ex_open = false;
	size = size_of_div_ex;
	for (i = 0; i < size; i++)
	{
		if (!div_ex[i].busy)
		{
			ex_open = true;
			break; // we have an open ex_unit so end loop
		}
	}

	// check reservation station to see if the instructions are ready
	if (ex_open)
	{
		size = size_of_mult_rs;
		for (int j = 0; j < size; j++)
		{
			unsigned rob_entry = mult_rs[j].dest;// gets the rob that the instruction is held in
			if (rob_entry >= 0 && rob_entry <= size_of_rob)
			{
				if (rob[rob_entry].state == "ISSUE")
				{

					if (station_ready(mult_rs[j]))//checks if we have all values necessary to compute
					{
						if (mult_rs[j].opcode == DIV || mult_rs[j].opcode == DIVS)
						{
							//if everything checks out then we move the instruction in the exe unit
							rob[rob_entry].state = "EXE";
							iq[rob_entry].Exe = clock_cycles;
							div_ex[i].busy = true;
							div_ex[i].ttf = div_ex[i].delay;
							div_ex[i].entry = mult_rs[j].dest;
							div_ex[i].opcode = mult_rs[j].opcode;
							div_ex[i].vj = mult_rs[j].vj;
							div_ex[i].vk = mult_rs[j].vk;
							div_ex[i].vjf = mult_rs[j].vjf;
							div_ex[i].vkf = mult_rs[j].vkf;
							div_ex[i].pc = mult_rs[j].pc;
							break;
						}
					}
				}
			}
		}
	}

	// decrement ttf of ex units
	size = size_of_int_ex;
	for (i = 0; i < size; i++)
	{
		if (int_ex[i].ttf != UNDEFINED)
		{
			int_ex[i].ttf--;
		}
	}
	size = size_of_add_ex;
	for (i = 0; i < size; i++)
	{
		if (add_ex[i].ttf != UNDEFINED)
		{
			add_ex[i].ttf--;
		}
	}
	size = size_of_mem_ex;
	for (i = 0; i < size; i++)
	{
		if (mem_ex[i].ttf != UNDEFINED)
		{
			mem_ex[i].ttf--;
		}
	}
	size = size_of_mult_ex;
	for (i = 0; i < size; i++)
	{
		if (mult_ex[i].ttf != UNDEFINED)
		{
			mult_ex[i].ttf--;
		}
	}
	size = size_of_div_ex;
	for (i = 0; i < size; i++)
	{
		if (div_ex[i].ttf != UNDEFINED)
		{
			div_ex[i].ttf--;
		}
	}

}

void sim_ooo::write_result()
{
	// check to see if any ex units are done
	int i;
	int size = size_of_int_ex;
	for (i = 0; i < size; i++)
	{
		if (int_ex[i].ttf == 0)
		{
			//clear ex unit after writing result
			int_ex[i] = clear_ex_unit(int_ex[i].name,int_ex[i].delay);
		}
		if (int_ex[i].ttf == 1)
		{
			int answer = compute_result_int(int_ex[i]);
			if (int_ex[i].opcode == BEQZ || int_ex[i].opcode == BNEZ || int_ex[i].opcode == BLTZ || int_ex[i].opcode == BGTZ
				|| int_ex[i].opcode == BLEZ || int_ex[i].opcode == BGEZ || int_ex[i].opcode == JUMP)
			{
				write_rob(answer, int_ex[i].entry);
			}
			else
			{
				write_rs(answer, int_ex[i].entry);
				write_rob(answer, int_ex[i].entry);
			}
			find_and_clear_rs(int_ex[i].pc);
		}
	}
	size = size_of_add_ex;
	for (i = 0; i < size; i++)
	{
		if (add_ex[i].ttf == 0)
		{
			//clear ex unit after writing result
			add_ex[i] = clear_ex_unit(add_ex[i].name,add_ex[i].delay);
		}
		if (add_ex[i].ttf == 1)
		{
			float answer = compute_result_fp(add_ex[i]);
			write_rs(answer, add_ex[i].entry);
			write_rob(answer, add_ex[i].entry);
			find_and_clear_rs(add_ex[i].pc);
		}
	}
	size = size_of_mem_ex;
	for (i = 0; i < size; i++)
	{
		if (mem_ex[i].ttf == 0)
		{
			//clear ex unit after writing result
			mem_ex[i] = clear_ex_unit(mem_ex[i].name,mem_ex[i].delay);
		}
		if (mem_ex[i].ttf == 1)
		{
			if (mem_ex[i].opcode == LW || mem_ex[i].opcode == SW || mem_ex[i].opcode == SWS)
			{
				int answer = compute_address_int(mem_ex[i]);
				if (mem_ex[i].opcode == LW)
				{
					write_rs(answer, mem_ex[i].entry);
				}
				write_rob(answer, mem_ex[i].entry);
			}
			else
			{
				float answer = compute_address_fp(mem_ex[i]);
				write_rs(answer, mem_ex[i].entry);
				write_rob(answer, mem_ex[i].entry);
			}
			find_and_clear_rs(mem_ex[i].pc);
		}
	}
	size = size_of_mult_ex;
	for (i = 0; i < size; i++)
	{
		if (mult_ex[i].ttf == 0)
		{
			//clear ex unit after writing result
			mult_ex[i] = clear_ex_unit(mult_ex[i].name,mult_ex[i].delay);
		}
		if (mult_ex[i].ttf == 1)
		{
			float answer = compute_result_fp(mult_ex[i]);
			write_rs(answer, mult_ex[i].entry);
			write_rob(answer, mult_ex[i].entry);
			find_and_clear_rs(mult_ex[i].pc);
		}
	}
	size = size_of_div_ex;
	for (i = 0; i < size; i++)
	{
		if (div_ex[i].ttf == 0)
		{
			//clear ex unit after writing result
			div_ex[i] = clear_ex_unit(div_ex[i].name,div_ex[i].delay);
		}
		if (div_ex[i].ttf == 1)
		{
			float answer = compute_result_fp(div_ex[i]);
			write_rs(answer, div_ex[i].entry);
			write_rob(answer, div_ex[i].entry);
			find_and_clear_rs(div_ex[i].pc);
		}
	}
}

void sim_ooo::commit()
{
	//find the lowest entry
	clear_write_back_check();
	for (int i = 0; i < issue_max; i++)
	{
		unsigned entry = rob[0].pc;
		unsigned pos = 0;
		int size = size_of_rob;
		for (int i = 0; i < size; i++)
		{
			if (rob[i].pc < entry)
			{
				entry = rob[i].pc;
				pos = i;
			}
		}
		if (rob[pos].ready)
		{
			unsigned opcode = (rob[pos].instruction >> 26) & 31;
			if (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ
				|| opcode == BGTZ || opcode == BLEZ || opcode == BGEZ)
			{
				if (rob[pos].value != UNDEFINED) // branch was taken
				{
					pc = rob[pos].value;
					flush_rob();
					flush_ex();
					flush_rs();
				}
			}
			else if (opcode == JUMP)
			{
				pc = rob[pos].value;
			}
			else if (opcode == SW)
			{
				int reg = convert_string_to_number(rob[pos].destination);
				write_memory(rob[pos].value, int_reg[reg].value);
			}
			else if (opcode == SWS)
			{
				int reg = convert_string_to_number(rob[pos].destination);
				write_memory(rob[pos].value, float2unsigned(fp_reg[reg].value));
			}
			else if (opcode == LW)
			{
				int reg = convert_string_to_number(rob[pos].destination);
				int_reg[reg].value = rob[pos].value;
				int_reg[reg].entry = UNDEFINED;
			}
			else if (opcode == LWS)
			{
				int reg = convert_string_to_number(rob[pos].destination);
				fp_reg[reg].value = rob[pos].value_f;
				fp_reg[reg].entry = UNDEFINED;
			}
			else if (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS)
			{
				int reg = convert_string_to_number(rob[pos].destination);
				fp_reg[reg].value = rob[pos].value_f;
				fp_reg[reg].entry = UNDEFINED;
			}
			else if (opcode == EOP)
			{
				eop = true;
				instruction_count--;
			}
			else
			{
				int reg = convert_string_to_number(rob[pos].destination);
				int_reg[reg].value = rob[pos].value;
				int_reg[reg].entry = UNDEFINED;
			}

			rob[pos] = clear_rob_entry(pos);
			iq[pos].pc = UNDEFINED;
			iq[pos].Issue = UNDEFINED;
			iq[pos].Exe = UNDEFINED;
			iq[pos].WR = UNDEFINED;
			iq[pos].Commit = UNDEFINED;
			instruction_count++;
		}
	}
}

int sim_ooo::get_open_rs(reservation_station *rs)
{
	int station = -1 , size;
	string name = rs[0].name.substr(0, rs[0].name.length() - 1);
	if (name == "Int")
	{
		size = size_of_int_rs;
	}
	else if (name == "Add")
	{
		size = size_of_add_rs;
	}
	else if (name == "Mult")
	{
		size = size_of_mult_rs;
	} 
	else if (name == "Load")
	{
		size = size_of_load_rs;
	}
	else
	{
		return station;
	}
	for (int i = 0; i < size; i++)
	{
		if (!rs[i].busy)
		{
			station = i;
			break;
		}
	}
	return station;
}

int sim_ooo::get_open_rob(read_order_buffer* rob)
{
	int station = -1;
	int size = size_of_rob;

	for (int i = open_rob_entry; i < size; i++)
	{
		if (!rob[i].busy)
		{
			station = i;
			open_rob_entry = station;
			return station;
		}
	}
	for (unsigned i = 0; i < open_rob_entry; i++)
	{
		if (!rob[i].busy)
		{
			station = i;
			open_rob_entry = station;
			return station;
		}
	}
	return station;
}

void sim_ooo::write_to_rob_issue(unsigned instruction, unsigned open_rob, unsigned pc, unsigned destination, bool int_or_float)
{
	string des;
	if (int_or_float)
	{
		des = "R";
		int_reg[destination].entry = open_rob;
	}
	else
	{
		des = "F";
		fp_reg[destination].entry = open_rob;
	}
	rob[open_rob].pc = pc;
	iq[open_rob].pc = pc;
	rob[open_rob].busy = true;
	rob[open_rob].ready = false;
	rob[open_rob].instruction = instruction;
	rob[open_rob].destination = des + to_string(destination);
	rob[open_rob].state = "ISSUE";
	iq[open_rob].Issue = clock_cycles;
	return;
}

void sim_ooo::write_to_rs(unsigned open_rs, unsigned rs, unsigned opcode, bool int_or_float, int vj, int vk, float vjf, float vkf, unsigned qj, unsigned qk, unsigned pc, unsigned a, unsigned open_rob)
{
	switch (rs)
	{
	case 1:
		int_rs[open_rs].busy = true;
		int_rs[open_rs].opcode = opcode;
		if (int_or_float)
		{
			int_rs[open_rs].vj = vj;
			int_rs[open_rs].vk = vk;
		}
		else
		{
			int_rs[open_rs].vjf = vjf;
			int_rs[open_rs].vkf = vkf;
		}
		int_rs[open_rs].qj = qj;
		int_rs[open_rs].qk = qk;
		int_rs[open_rs].dest = open_rob;
		int_rs[open_rs].a = a;
		int_rs[open_rs].pc = pc;
		break;
	case 2:
		add_rs[open_rs].busy = true;
		add_rs[open_rs].opcode = opcode;
		if (int_or_float)
		{
			add_rs[open_rs].vj = vj;
			add_rs[open_rs].vk = vk;
		}
		else
		{
			add_rs[open_rs].vjf = vjf;
			add_rs[open_rs].vkf = vkf;
		}
		add_rs[open_rs].qj = qj;
		add_rs[open_rs].qk = qk;
		add_rs[open_rs].dest = open_rob;
		add_rs[open_rs].a = a;
		add_rs[open_rs].pc = pc;
		break;
	case 3:
		mult_rs[open_rs].busy = true;
		mult_rs[open_rs].opcode = opcode;
		if (int_or_float)
		{
			mult_rs[open_rs].vj = vj;
			mult_rs[open_rs].vk = vk;
		}
		else
		{
			mult_rs[open_rs].vjf = vjf;
			mult_rs[open_rs].vkf = vkf;
		}
		mult_rs[open_rs].qj = qj;
		mult_rs[open_rs].qk = qk;
		mult_rs[open_rs].dest = open_rob;
		mult_rs[open_rs].a = a;
		mult_rs[open_rs].pc = pc;
		break;
	case 4:
		load_rs[open_rs].busy = true;
		load_rs[open_rs].opcode = opcode;
		load_rs[open_rs].vj = vj;
		load_rs[open_rs].qj = qj;
		load_rs[open_rs].dest = open_rob;
		load_rs[open_rs].a = a;
		load_rs[open_rs].pc = pc;
		break;
	}
}

unsigned sim_ooo::get_q(unsigned i, bool int_or_float)
{
	if (int_or_float)
	{
		unsigned rob_entry = get_pending_int_register(i);
		if (rob_entry != UNDEFINED)
		{
			if (rob[rob_entry].ready)
			{
				return UNDEFINED;
			}
			else
			{
				return rob_entry;
			}
		}
		return UNDEFINED;
	}
	else
	{
		unsigned rob_entry = get_pending_fp_register(i);
		if (rob_entry != UNDEFINED)
		{
			if (rob[rob_entry].ready)
			{
				return UNDEFINED;
			}
			else
			{
				return rob_entry;
			}
		}
		return UNDEFINED;
	}
}

bool sim_ooo::station_ready(reservation_station rs)
{
	bool ready = false;
	if ((rs.qj < 0 || rs.qj > size_of_rob) && (rs.qk < 0 || rs.qk > size_of_rob))
	{
		if (!rs.wb)//checks to see if qj or qk have been written back this cycle
		{
			ready = true;
		}
	}
	return ready;
}

int sim_ooo::compute_result_int(ex_unit ex)
{
	int answer = UNDEFINED;
	unsigned opcode = ex.opcode;
	switch (opcode)
	{
	case ADD:
		answer = ex.vj + ex.vk;
		break;
	case SUB:
		answer = ex.vj - ex.vk;
		break;
	case XOR:
		answer = ex.vj ^ ex.vk;
		break;
	case OR:
		answer = ex.vj | ex.vk;
		break;
	case AND:
		answer = ex.vj & ex.vk;
		break;
	case ADDI:
		answer = ex.vj + ex.vk;
		break;
	case SUBI:
		answer = ex.vj - ex.vk;
		break;
	case XORI:
		answer = ex.vj ^ ex.vk;
		break;
	case ORI:
		answer = ex.vj | ex.vk;
		break;
	case ANDI:
		answer = ex.vj & ex.vk;
		break;
	case BEQZ:
		if (ex.vj == 0)
		{
			answer = ex.vk;
		}
		break;
	case BNEZ:
		if (ex.vj != 0)
		{
			answer = ex.vk;
		}
		break;
	case BLTZ:
		if (ex.vj < 0)
		{
			answer = ex.vk;
		}
		break;
	case BGTZ:
		if (ex.vj > 0)
		{
			answer = ex.vk;
		}
		break;
	case BLEZ:
		if (ex.vj <= 0)
		{
			answer = ex.vk;
		}
		break;
	case BGEZ:
		if (ex.vj >= 0)
		{
			answer = ex.vk;
		}
		break;
	case JUMP:
		answer = ex.vk;
		break;
	case MULT:
		answer = ex.vj * ex.vk;
		break;
	case DIV:
		answer = ex.vj / ex.vk;
		break;
	}
	return answer;
}

float sim_ooo::compute_result_fp(ex_unit ex)
{
	float answer = 0;
	unsigned opcode = ex.opcode;
	switch (opcode)
	{
	case ADDS:
		answer = ex.vjf + ex.vkf;
		break;
	case SUBS:
		answer = ex.vjf - ex.vkf;
		break;
	case MULTS:
		answer = ex.vjf * ex.vkf;
		break;
	case DIVS:
		answer = ex.vjf / ex.vkf;
		break;
	}
	return answer;
}

int sim_ooo::compute_address_int(ex_unit ex)
{
	int answer = 0;
	if (ex.opcode == LW)
	{
		answer += data_memory[ex.vj + ex.vk + 3];
		answer = answer << 8;
		answer += data_memory[ex.vj + ex.vk + 2];
		answer = answer << 8;
		answer += data_memory[ex.vj + ex.vk + 1];
		answer = answer << 8;
		answer += data_memory[ex.vj + ex.vk + 0];
	}
	else
	{
		answer = ex.vj + ex.vk;
	}
	return answer;
}

float sim_ooo::compute_address_fp(ex_unit ex)
{
	float ans;
	int answer = 0;
	if (ex.opcode == LWS)
	{
		answer += data_memory[ex.vj + ex.vk + 3];
		answer = answer << 8;
		answer += data_memory[ex.vj + ex.vk + 2];
		answer = answer << 8;
		answer += data_memory[ex.vj + ex.vk + 1];
		answer = answer << 8;
		answer += data_memory[ex.vj + ex.vk + 0];
		ans = unsigned2float(answer);
	}
	return ans;
}

void sim_ooo::write_rs(int answer, unsigned dest)
{
	int size, i;
	size = size_of_int_rs;
	for (i = 0; i < size; i++)
	{
		if (int_rs[i].qj == dest)
		{
			int_rs[i].qj = UNDEFINED;
			int_rs[i].vj = answer;
			int_rs[i].wb = true;
		}
		if (int_rs[i].qk == dest)
		{
			int_rs[i].qk = UNDEFINED;
			int_rs[i].vk = answer;
			int_rs[i].wb = true;
		}
	}
	size = size_of_add_rs;
	for (i = 0; i < size; i++)
	{
		if (add_rs[i].qj == dest)
		{
			add_rs[i].qj = UNDEFINED;
			add_rs[i].vj = answer;
			add_rs[i].wb = true;
		}
		if (add_rs[i].qk == dest)
		{
			add_rs[i].qk = UNDEFINED;
			add_rs[i].vk = answer;
			add_rs[i].wb = true;
		}
	}
	size = size_of_mult_rs;
	for (i = 0; i < size; i++)
	{
		if (mult_rs[i].qj == dest)
		{
			mult_rs[i].qj = UNDEFINED;
			mult_rs[i].vj = answer;
			mult_rs[i].wb = true;
		}
		if (mult_rs[i].qk == dest)
		{
			mult_rs[i].qk = UNDEFINED;
			mult_rs[i].vk = answer;
			mult_rs[i].wb = true;
		}
	}
	size = size_of_load_rs;
	for (i = 0; i < size; i++)
	{
		if (load_rs[i].qj == dest)
		{
			load_rs[i].qj = UNDEFINED;
			load_rs[i].vj = answer;
			load_rs[i].wb = true;
		}
		if (load_rs[i].qk == dest)
		{
			load_rs[i].qk = UNDEFINED;
			load_rs[i].vk = answer;
			load_rs[i].wb = true;
		}
	}
}

void sim_ooo::write_rs(float answer, unsigned dest)
{
	int size, i;
	size = size_of_int_rs;
	for (i = 0; i < size; i++)
	{
		if (int_rs[i].qj == dest)
		{
			int_rs[i].qj = UNDEFINED;
			int_rs[i].vjf = answer;
			int_rs[i].wb = true;
		}
		if (int_rs[i].qk == dest)
		{
			int_rs[i].qk = UNDEFINED;
			int_rs[i].vkf = answer;
			int_rs[i].wb = true;
		}
	}
	size = size_of_add_rs;
	for (i = 0; i < size; i++)
	{
		if (add_rs[i].qj == dest)
		{
			add_rs[i].qj = UNDEFINED;
			add_rs[i].vjf = answer;
			add_rs[i].wb = true;
		}
		if (add_rs[i].qk == dest)
		{
			add_rs[i].qk = UNDEFINED;
			add_rs[i].vkf = answer;
			add_rs[i].wb = true;
		}
	}
	size = size_of_mult_rs;
	for (i = 0; i < size; i++)
	{
		if (mult_rs[i].qj == dest)
		{
			mult_rs[i].qj = UNDEFINED;
			mult_rs[i].vjf = answer;
			mult_rs[i].wb = true;
		}
		if (mult_rs[i].qk == dest)
		{
			mult_rs[i].qk = UNDEFINED;
			mult_rs[i].vkf = answer;
			mult_rs[i].wb = true;
		}
	}
	size = size_of_load_rs;
	for (i = 0; i < size; i++)
	{
		if (load_rs[i].qj == dest)
		{
			load_rs[i].qj = UNDEFINED;
			load_rs[i].vjf = answer;
			load_rs[i].wb = true;
		}
		if (load_rs[i].qk == dest)
		{
			load_rs[i].qk = UNDEFINED;
			load_rs[i].vkf = answer;
			load_rs[i].wb = true;
		}
	}
}

void sim_ooo::write_rob(int answer, unsigned entry)
{
	rob[entry].value = answer;
	rob[entry].state = "WR";
	iq[entry].WR = clock_cycles;
	rob[entry].ready = true;
}

void sim_ooo::write_rob(float answer, unsigned entry)
{
	rob[entry].value = float2unsigned(answer);
	rob[entry].value_f = answer;
	rob[entry].state = "WR";
	iq[entry].WR = clock_cycles;
	rob[entry].ready = true;
}

ex_unit sim_ooo::clear_ex_unit(std::string name, unsigned delay)
{
	ex_unit ex;
	ex.name = name;
	ex.busy = false;
	ex.entry = UNDEFINED;
	ex.opcode = UNDEFINED;
	ex.ttf = UNDEFINED;
	ex.delay = delay;
	ex.vj = UNDEFINED;
	ex.vk = UNDEFINED;
	ex.vjf = (float)UNDEFINED;
	ex.vkf = (float)UNDEFINED;
	return ex;
}

void sim_ooo::flush_rob()
{
	int size = size_of_rob;
	for (int i = 0; i < size; i++)
	{
		rob[i].entry = 1+i;
		rob[i].busy = false;
		rob[i].destination = "";
		rob[i].pc = UNDEFINED;
		rob[i].instruction = UNDEFINED;
		rob[i].ready = false;
		rob[i].state = "";
		rob[i].value = UNDEFINED;
		rob[i].value_f = (float)UNDEFINED;
		open_rob_entry = 0;

		iq[i].pc = UNDEFINED;
		iq[i].Issue = UNDEFINED;
		iq[i].Exe = UNDEFINED;
		iq[i].WR = UNDEFINED;
		iq[i].Commit = UNDEFINED;
	}
}

read_order_buffer sim_ooo::clear_rob_entry(unsigned entry)
{
	read_order_buffer empty;
	empty.entry = entry;
	empty.busy = false;
	empty.destination = "";
	empty.pc = UNDEFINED;
	empty.instruction = UNDEFINED;
	empty.ready = false;
	empty.state = "";
	empty.value = UNDEFINED;
	empty.value_f = (float)UNDEFINED;
	return empty;
}

void sim_ooo::flush_ex()
{
	int size = size_of_int_ex;
	for (int i = 0; i < size; i++)
	{
		int_ex[i] = clear_ex_unit(int_ex[i].name,int_ex[i].delay);
	}
	size = size_of_add_ex;
	for (int i = 0; i < size; i++)
	{
		add_ex[i] = clear_ex_unit(add_ex[i].name, add_ex[i].delay);
	}
	size = size_of_mult_ex;
	for (int i = 0; i < size; i++)
	{
		mult_ex[i] = clear_ex_unit(mult_ex[i].name, mult_ex[i].delay);
	}
	size = size_of_div_ex;
	for (int i = 0; i < size; i++)
	{
		div_ex[i] = clear_ex_unit(div_ex[i].name, div_ex[i].delay);
	}
	size = size_of_mem_ex;
	for (int i = 0; i < size; i++)
	{
		mem_ex[i] = clear_ex_unit(mem_ex[i].name, mem_ex[i].delay);
	}
}

void sim_ooo::flush_rs()
{
	int size = size_of_int_rs;
	for (int i = 0; i < size; i++)
	{
		int_rs[i] = clear_rs(int_rs[i].name);
	}
	size = size_of_add_rs;
	for (int i = 0; i < size; i++)
	{
		add_rs[i] = clear_rs(add_rs[i].name);
	}
	size = size_of_mult_rs;
	for (int i = 0; i < size; i++)
	{
		mult_rs[i] = clear_rs(mult_rs[i].name);
	}
	size = size_of_load_rs;
	for (int i = 0; i < size; i++)
	{
		load_rs[i] = clear_rs(load_rs[i].name);
	}
}

reservation_station sim_ooo::clear_rs(std::string name)
{
	reservation_station rs;
	rs.name = name;
	rs.a = UNDEFINED;
	rs.pc = UNDEFINED;
	rs.busy = false;
	rs.dest = UNDEFINED;
	rs.opcode = UNDEFINED;
	rs.qj = UNDEFINED;
	rs.qk = UNDEFINED;
	rs.vj = UNDEFINED;
	rs.vk = UNDEFINED;
	rs.vjf = unsigned2float(UNDEFINED);
	rs.vkf = unsigned2float(UNDEFINED);
	rs.wb = false;
	return rs;
}

void sim_ooo::find_and_clear_rs(unsigned pc)
{
	int size = size_of_int_rs;
	for (int i = 0; i < size; i++)
	{
		if (int_rs[i].pc == pc)
		{
			int_rs[i] = clear_rs(int_rs[i].name);
			return;
		}
	}
	size = size_of_add_rs;
	for (int i = 0; i < size; i++)
	{
		if (add_rs[i].pc == pc)
		{
			add_rs[i] = clear_rs(add_rs[i].name);
			return;
		}
	}
	size = size_of_mult_rs;
	for (int i = 0; i < size; i++)
	{
		if (mult_rs[i].pc == pc)
		{
			mult_rs[i] = clear_rs(mult_rs[i].name);
			return;
		}
	}
	size = size_of_load_rs;
	for (int i = 0; i < size; i++)
	{
		if (load_rs[i].pc == pc)
		{
			load_rs[i] = clear_rs(load_rs[i].name);
			return;
		}
	}
}

void sim_ooo::clear_write_back_check()
{
	int size, i;
	size = size_of_int_rs;
	for (i = 0; i < size; i++)
	{
		int_rs[i].wb = false;
	}
	size = size_of_add_rs;
	for (i = 0; i < size; i++)
	{
		add_rs[i].wb = false;
	}
	size = size_of_mult_rs;
	for (i = 0; i < size; i++)
	{
		mult_rs[i].wb = false;
	}
	size = size_of_load_rs;
	for (i = 0; i < size; i++)
	{
		load_rs[i].wb = false;
	}
}