#include <iostream>
#include "filereader.h"

struct Foo {
    std::string name;
    std::string relations;
};

int main() {
    std::string source = "name = foo, relations = bad";
    auto obj = read_object<Foo>(source,
    [] (std::string name, std::string relations) {
        Foo foo{};
        foo.name = name;
        foo.relations = relations;
        return foo;
    }, std::make_tuple (
        converter<std::string>{
            "name", [] (std::string & str) {
                            auto name = str.substr(0, str.find_first_of(','));
                            str.erase(0, str.find_first_of(',') + 1);
                            return name;
            }
        },
        converter<std::string>{
            "relations", [] (std::string & str) {
                            auto pos = str.find_first_of(',');
                            auto name = str.substr(0, pos);
                            if (pos != std::string::npos)
                                str.erase(0, pos + 1);
                            else str.clear();
                            return name;
            }
        }
    ));
    std::cout << obj.name << " | " << obj.relations << std::endl;
    return 0;
}
