#include "logd.hpp"

namespace logd {

void logd(std::string str) {
    std::cout << str << std::endl;
}

void logd(std::string str, std::initializer_list<variant> l) {
    std::cout << translate(str, l) << std::endl;
}

void logd_err(std::string str) {
    std::cerr << str << std::endl;
}

void logd_err(std::string str, std::initializer_list<variant> l) {
    std::cerr << translate(str, l) << std::endl;
}

std::string translate(std::string src, std::initializer_list<variant> l) {
    std::vector<std::string> d;
    for(const variant &t : l) d.push_back(t.toString());
    size_t lpos = 0, pos, tmax = d.size();
    while((pos = src.find('$', lpos)) != std::string::npos) {
        if(pos+1 < src.length() && src[pos+1] == '$') { //Translate "$$" into "$"
            src.replace(pos, 2, "$");
            lpos = pos + 1;
            continue;
        }
        std::string rp;
        try {
            int t = std::stoi(src.substr(pos+1, 1));
            if(t > tmax) rp = "{index out of bound}";
                else rp = d[t];
        } catch(...) {
            rp = "{exception thrown}";
        }

        src.replace(pos, 2, rp);
        lpos = pos;
    }
    return src;
}

};