#ifndef PDF_UTILITY_H
#define PDF_UTILITY_H

#include <iomanip>
#include <ios>
#include <istream>
#include <map>
#include <vector>

namespace util {

// return the size in bytes of the stream from the current position.
// restores the current position in the stream
size_t bytes_till_end(std::istream &is);

// read a file as bytes into a character vector
std::vector<char> slurp_bytes(const std::string &filename);

// decompress a given number of bytes in an input stream
// restores the current position in the stream
std::vector<char> inflate_stream(std::istream &is, std::streamoff);

// Prototype Stuff ////////////////////////////////////////////////////////////
// I am going to prototype some types and parsing functions in here to keep
// experimentation simple. I will move them to proper files and classes and
// make them more robust when things stabalize a bit.

// NOTE I am going to stash some object classes in here for now too while
// I prototype. They should be in their own files once we figure out how to
// set them up properly.

// Abstract class for all pdf objects, keeping everything public for now until
// things are more settled and I can make them more robust without having to
// change them right after and do a lot of work twice.
class PdfObj {
public:
  virtual ~PdfObj() {}
  virtual void write(std::ostream &os) const = 0;
};

class PdfNull : public PdfObj {
public:
  virtual void write(std::ostream &os) const override { os << "null"; }
};

class PdfString : public PdfObj {
public:
  std::string data;
  PdfString(const std::string &s) : data(s) {}
  virtual void write(std::ostream &os) const override {
    os << "(" << data << ")";
  }
};

class PdfName : public PdfObj {
public:
  std::string data;
  PdfName(const std::string &s) : data(s) {} // Validate?
  virtual void write(std::ostream &os) const override { os << data; }
  bool operator<(const PdfName &other) const { return data < other.data; }
  bool operator<(const PdfName &other) { return data < other.data; }
};

class PdfInt : public PdfObj {
public:
  int64_t data;
  PdfInt(int64_t i) : data(i) {}
  virtual void write(std::ostream &os) const override { os << data; }
};

class PdfReal : public PdfObj {
public:
  double data;
  PdfReal(double d) : data(d) {}
  virtual void write(std::ostream &os) const override {
    os << std::setprecision(15) << data;
  }
};

class PdfBool : public PdfObj {
public:
  bool data;
  PdfBool(bool b) : data(b) {}
  virtual void write(std::ostream &os) const override {
    os << (data ? "true" : "false");
  }
};

class PdfArray : public PdfObj {
public:
  std::vector<PdfObj *> objects;
  virtual ~PdfArray() {
    for (auto obj : objects)
      delete obj;
  }
  virtual void write(std::ostream &os) const override {
    os << "[ ";
    for (auto obj : objects) {
      obj->write(os);
      os << " ";
    }
    os << "]";
  }
};

class PdfDict : public PdfObj {
public:
  std::map<PdfName, PdfObj *> pairs;
  virtual void write(std::ostream &os) const override {
    os << "<< ";
    for (auto &p : pairs) {
      p.first.write(os);
      os << " ";
      p.second->write(os);
      os << " ";
    }
    os << ">>";
  }
};

class PdfStream : public PdfObj {
public:
  PdfDict dict;
  std::vector<char> stream;
  virtual void write(std::ostream &os) const override {
    dict.write(os);
    os << "\nstream\n";
    for (auto ch : stream) {
      os << ch;
    }
    // Should this start with an endline? or would that mess up compression
    os << "\nendstream\n";
  }
};

class PdfRef : public PdfObj {
public:
  int64_t num;
  int64_t gen;
  PdfRef(int64_t n, int64_t g) : num(n), gen(g) {}
  virtual void write(std::ostream &os) const override {
    os << num << " " << gen << " R";
  }
};

// This is the object for an object with a number generation and a PdfObj
class PdfTopLevel : PdfObj {
public:
  int64_t num;
  int64_t gen;
  PdfObj *obj;
  PdfTopLevel(int64_t n, int64_t g, PdfObj *o) : num(n), gen(g), obj(o) {}
  virtual ~PdfTopLevel() { delete obj; }
  virtual void write(std::ostream &os) const override {
      os << num << " " << gen << " obj\n";
      obj->write(os);
      os << "\nendobj\n";
    }
};

// NOTE the functions below are just prototypes and should go in the parser
// class once it is proper. I don't really know enough about the PDF format
// and how I would want to parse a whole document yet to build a nice parser
// object. This way I can experiement and change the way things are done
// without worrying about architecture and then work on putting them into
// a nice class with a good interface later.

// parse a pdf object "num gen obj ... endobj"
PdfTopLevel parse_top_level_obj(std::istream &is);

// parse any object and return a pointer to it
// it will likely be necessary to have other finer grained functions to get
// specific object types where necessary
PdfObj *parse_pdf_obj(std::istream &is);

} // namespace util

#endif
