#include <pybind11/pybind11.h>
#include "PieceTable.h"
#include "History.h" // Import our new History Manager!

namespace py = pybind11;

PYBIND11_MODULE(vrutti_core, m) {
    
    // 1. Expose the PieceTable
    py::class_<PieceTable>(m, "PieceTable")
        .def(py::init<const std::string&>())
        .def("insert_text", &PieceTable::insert_text)
        .def("delete_text", &PieceTable::delete_text) // Pybind11 automatically handles the std::string return!
        .def("get_text", &PieceTable::get_text)
        .def("length", &PieceTable::length);

    // 2. Expose the ActionType Enum
    py::enum_<ActionType>(m, "ActionType")
        .value("Insert", ActionType::Insert)
        .value("Delete", ActionType::Delete)
        .export_values();

    // 3. Expose the Action Struct (Read-Only)
    py::class_<Action>(m, "Action")
        .def_readonly("type", &Action::type)
        .def_readonly("index", &Action::index)
        .def_readonly("text", &Action::text);

    // 4. Expose the History Manager
    py::class_<HistoryManager>(m, "HistoryManager")
        .def(py::init<>())
        .def("record", &HistoryManager::record)
        .def("can_undo", &HistoryManager::can_undo)
        .def("can_redo", &HistoryManager::can_redo)
        .def("pop_undo", &HistoryManager::pop_undo)
        .def("pop_redo", &HistoryManager::pop_redo)
        .def("clear", &HistoryManager::clear);
}