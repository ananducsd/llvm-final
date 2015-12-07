#include <map>
#include <string.h>
#include <utility>
#include <llvm/IR/Instruction.h>
#include <iostream>
#include <string>
#include "stdio.h"

std::map<std::string, int> glob_map;
std::map<std::string, int>::iterator iter;

extern "C" {

	void update_biasmap(char* fn_name, int is_true) {
		std::string key(fn_name);
		
		std::cout << "got a branch update " << fn_name << " " <<  is_true <<"\n" ;
		if(is_true == 1) {
		    glob_map[key] += 1;    
		
		}
		//std::cout << "opcode = " << op_code << ", count = " << count <<"\n";
		/*iter = glob_map.find(op_code); 
		if (iter != glob_map.end()) {
			iter->second = iter->second + count;
		} else {
			glob_map.insert(std::make_pair(op_code, count));	
		}*/	
	}

	void print_biasmap() {
		int total = 0;
		for(iter = glob_map.begin(); iter != glob_map.end(); ++iter) {
					
			std::cout << iter->first << "\t" << iter->second << "\n";
			total += iter->second;
		}
		
                std::cout << "Total\t" << total << "\n";
	}

}
