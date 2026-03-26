#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

class Greeter {
private:
    std::string name;
    
    // This is the private method. Python cannot see or call this directly.
    std::string greet() {
        return "hello " + name;
    }

public:
    // The constructor takes the string argument
    Greeter(const std::string& user_name) : name(user_name) {}

    // The public bridge that Python will call, which triggers the private method
    std::string say_hello() {
        return greet();
    }
};

// This block is the magic. It tells pybind11 exactly what to expose to Python.
PYBIND11_MODULE(hello, m) {
    py::class_<Greeter>(m, "Greeter")
        .def(py::init<const std::string &>()) // Binds the constructor
        .def("say_hello", &Greeter::say_hello); // Binds the public method
}