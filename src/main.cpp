#include <iostream>
#include "filereader.h"

struct Foo {
    std::string name;
    int relations;
    std::vector<int> list;
};

int main() {
    std::string source = "{name = \"foo\", relations = 5, list = {1, 2, 3}}";
    auto factory = [] (std::string name, int relations, std::vector<int> list) {
        return Foo {name, relations, list};
    };
    auto obj = read_object<Foo>(source, factory, std::make_tuple (
        converter<std::string>{"name", read_string},
        converter<int>{"relations", read_int},
        converter<std::vector<int>>{"list", std::bind(read_list<int>, std::placeholders::_1, read_int)}
    ));
    std::cout << obj.name << " | " << obj.relations << std::endl;
    for (auto it : obj.list) {
        std::cout << it << " ";
    }
    return 0;
}
