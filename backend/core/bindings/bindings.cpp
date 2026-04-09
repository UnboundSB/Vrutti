#include <pybind11/pybind11.h>
#include "../include/PieceTable.h" // We have to step out of the bindings folder to find the header!

namespace py = pybind11;

// "vrutti_core" is the name we put in setup.py. 
// When you run 'import vrutti_core' in Python, this block executes.
PYBIND11_MODULE(vrutti_core, m) {
    
    py::class_<PieceTable>(m, "PieceTable")
        
        // 1. Bind the Constructor (Already done for you!)
        .def(py::init<const std::string&>())
        
        // STEP 1: Bind the get_text method
        // Python name: "get_text"
        // C++ Address: &PieceTable::get_text
       .def("get_text",&PieceTable::get_text)
        
        // STEP 2: Bind the insert_text method
        // Python name: "insert_text"
        .def("insert_text",&PieceTable::insert_text)
        
        // STEP 3: Bind the length method
        // Python name: "length"
        .def("length",&PieceTable::length)
        .def("delete_text", &PieceTable::delete_text)
        ; // <-- Notice the semicolon is at the very end of the chain!
}