#include "test.hpp"
#include <iostream>
int main(){
    int failures=0;
    for(const auto& t:test_registry()){
        try{t.function();std::cout<<"[PASS] "<<t.name<<'\n';}
        catch(const std::exception& e){++failures;std::cerr<<"[FAIL] "<<t.name<<": "<<e.what()<<'\n';}
    }
    std::cout<<"\n"<<(test_registry().size()-static_cast<std::size_t>(failures))<<'/'<<test_registry().size()<<" tests passed.\n";
    return failures==0?0:1;
}
