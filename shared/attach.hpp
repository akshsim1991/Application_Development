#pragma once
// Files attached to a document (view, add, save a copy, remove), and its layers (optional content: shown or hidden).
#include <string>
#include <utility>
#include <vector>
#include "document.hpp"

namespace attach {

struct File {
    std::wstring name;
    size_t size = 0;
};

std::vector<File> list(Document& d);
bool get(Document& d, int index, std::vector<unsigned char>& out);
// Each is one undo step.
bool add(Document& d, const std::wstring& name, const std::vector<unsigned char>& data);
bool remove(Document& d, int index);

} // namespace attach

namespace layers {

struct Layer {
    int object = -1;       // the layer's object number in the file
    std::wstring name;
    bool visible = true;   // in the document's default view
};

// From a complete file as written by PDFium.
std::vector<Layer> list(const std::vector<unsigned char>& pdf);
// The file with the layers' default visibility changed (object number -> shown). False when the structure is not supported.
bool setVisible(const std::vector<unsigned char>& pdf, const std::vector<std::pair<int, bool>>& changes, std::vector<unsigned char>& out);

std::vector<Layer> list(Document& d);
// Changes the document (one undo step); the pages are drawn with the new visibility.
bool setVisible(Document& d, const std::vector<std::pair<int, bool>>& changes);

// A PDF string token ("(text)" or "<FEFF...>") as text.
std::wstring decodeString(const std::string& token);

} // namespace layers
