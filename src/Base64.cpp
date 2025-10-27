#include "Base64.hpp"

#include <string.h>

namespace Base64 {

int index(const char c) {
    std::string code= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; int ic;
    return ((ic=static_cast<int>(code.find(c)))==-1?0:ic);
}

std::string encode(unsigned char *input , size_t input_len) {
    const char * code="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char input_char[3];
    char output_char[5];
    int output_num;
    std::string output_str="";
    int near=input_len%3;
    for (int i=0;i<input_len;i+=3) {
        memset(input_char,0,3);
        memset(output_char,61,5);
        if (i+3 <= input_len)
            memcpy(input_char,input+i,3);
        else {
            memcpy(input_char,input+i,input_len-i);
            output_num=((int)input_char[0]<<16)+((int)input_char[1]<<8)+(int)input_char[2];
            if (near==1) {
                output_char[0]=code[((output_num>>18) & 0x3f)];
                output_char[1]=code[((output_num>>12) & 0x3f)];
                output_char[2]='='; output_char[3]='='; output_char[4]='\0';
            } else if (near==2) {
                output_char[0]=code[((output_num>>18) & 0x3f)];
                output_char[1]=code[((output_num>>12) & 0x3f)];
                output_char[2]=code[((output_num>>6) & 0x3f)];
                output_char[3]='='; output_char[4]='\0';
            }
            output_str.append(output_char);
            break;
        }
        output_num=((int)input_char[0]<<16)+((int)input_char[1]<<8)+(int)input_char[2];
        output_char[0]=code[((output_num>>18) & 0x3f)];
        output_char[1]=code[((output_num>>12) & 0x3f)];
        output_char[2]=code[((output_num>>6) & 0x3f)];
        output_char[3]=code[((output_num) & 0x3f)];
        output_char[4]='\0';
        output_str.append(output_char);
    }
    return output_str;
}

std::string decode(std::string input) {
	unsigned char input_char[4];
	unsigned char output_char[4];
	int output_num=0;
	int k=0;
	std::string output_str="";
	for (unsigned int i=0;i<input.size();i++) {
		input_char[k++]=index(input[i]);
		if (k==4) {
			output_num= ((int)input_char[0]<<18)+((int)input_char[1]<<12)+((int)input_char[2]<<6)+((int)input_char[3]);
			output_char[0]=(unsigned char)((output_num & 0x00FF0000)>> 16) ;
			output_char[1]=(unsigned char)((output_num & 0x0000FF00) >> 8) ;
			output_char[2]=(unsigned char)(output_num & 0x000000FF);
			output_char[3]='\0';
			output_str.append((char *)output_char);
			k=0;
		}
	}
	return output_str;
}


} // namespace Base64