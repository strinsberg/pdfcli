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

// Object Prototypes //////////////////////////////////////////////////////////

// I am going to prototype some types and parsing functions in here to keep
// experimentation simple. I will move them to proper files and classes and
// make them more robust when things stabalize a bit.

// Abstract class for all pdf objects, keeping everything public for now until
// things are more settled and I can make them more robust without having to
// change them right after and do a lot of work twice.
class PdfObj {
public:
  virtual ~PdfObj() {}
  virtual void write(std::ostream &os) const = 0;
  virtual bool operator==(const PdfObj &other) const = 0;
  virtual bool operator!=(const PdfObj &other) const {
    return !(*this == other);
  };
};

class PdfNull : public PdfObj {
public:
  virtual void write(std::ostream &os) const override { os << "null"; }
  virtual bool operator==(const PdfObj &other) const override {
    if (const PdfNull *null = dynamic_cast<const PdfNull *>(&other)) {
      return true;
    }
    return false;
  }
};

class PdfString : public PdfObj {
public:
  std::string data;
  PdfString(const std::string &s) : data(s) {}
  virtual void write(std::ostream &os) const override {
    os << "(" << data << ")";
  }
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfString *other = dynamic_cast<const PdfString *>(&obj)) {
      return data == other->data;
    }
    return false;
  }
};

class PdfName : public PdfObj {
public:
  std::string data;
  PdfName(const std::string &s) : data(s) {} // Validate?
  virtual void write(std::ostream &os) const override { os << data; }
  bool operator<(const PdfName &other) const { return data < other.data; }
  bool operator<(const PdfName &other) { return data < other.data; }
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfName *other = dynamic_cast<const PdfName *>(&obj)) {
      return data == other->data;
    }
    return false;
  }
};

class PdfInt : public PdfObj {
public:
  int64_t data;
  PdfInt(int64_t i) : data(i) {}
  virtual void write(std::ostream &os) const override { os << data; }
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfInt *other = dynamic_cast<const PdfInt *>(&obj)) {
      return data == other->data;
    }
    return false;
  }
};

class PdfReal : public PdfObj {
public:
  double data;
  PdfReal(double d) : data(d) {}
  virtual void write(std::ostream &os) const override {
    os << std::setprecision(15) << data;
  }
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfReal *other = dynamic_cast<const PdfReal *>(&obj)) {
      return data == other->data;
    }
    return false;
  }
};

class PdfBool : public PdfObj {
public:
  bool data;
  PdfBool(bool b) : data(b) {}
  virtual void write(std::ostream &os) const override {
    os << (data ? "true" : "false");
  }
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfBool *other = dynamic_cast<const PdfBool *>(&obj)) {
      return data == other->data;
    }
    return false;
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
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfArray *other = dynamic_cast<const PdfArray *>(&obj)) {
      if (objects.size() != other->objects.size())
        return false;
      for (size_t i = 0; i < objects.size(); ++i) {
        if (*objects.at(i) != *other->objects.at(i))
          return false;
      }
    }
    return true;
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
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfDict *other = dynamic_cast<const PdfDict *>(&obj)) {
      if (pairs.size() != other->pairs.size()) {
        return false;
      } else {
        for (auto &p : pairs) {
          auto it = other->pairs.find(p.first);
          if (it == other->pairs.end() || *it->second != *p.second)
            return false;
        }
      }
    }
    return true;
  }
};

class PdfStream : public PdfObj {
public:
  PdfDict *dict;
  std::vector<char> stream;
  PdfStream() : dict(new PdfDict) {}
  PdfStream(PdfDict *d) : dict(d) {}
  virtual ~PdfStream() { delete dict; }
  virtual void write(std::ostream &os) const override {
    dict->write(os);
    os << "\nstream\n";
    for (auto ch : stream) {
      os << ch;
    }
    // Should this start with an endline? or would that mess up compression
    os << "\nendstream\n";
  }
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfStream *other = dynamic_cast<const PdfStream *>(&obj)) {
      return *dict == *other->dict && stream == other->stream;
    }
    return false;
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
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfRef *other = dynamic_cast<const PdfRef *>(&obj)) {
      return num == other->num && gen == other->gen;
    }
    return false;
  }
};

// TODO This should be renamed indirect object instead
// TODO can these be presented as obj num gen <object> endobj ??? if so then
// I will need to add some logic to make both possible.
// NOTE these should only be able to appear in 2 places, as top level objects
// in the document or in a stream where the /Type is /ObjStream. Current
// parsing does not care and would happily parse one in the middle of an array
// so it will be necessary to provide this context at some point.
class PdfTopLevel : public PdfObj {
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
  virtual bool operator==(const PdfObj &obj) const override {
    if (const PdfTopLevel *other = dynamic_cast<const PdfTopLevel *>(&obj)) {
      return num == other->num && gen == other->gen &&
             *this->obj == *other->obj;
    }
    return false;
  }
};

// TODO consider how we will represent the cross-reference table and/or the
// trailer. Should they be separate types in a pdf document, or should they
// be a kind of pdf object that can be determined by it's dictionary values.
// In my example PDF the xref is just an indirect object stream object and has
// the /Type /XRef. It could be possible to parse this as an object stream and
// to then compose a new type that holds a PdfTopLevel (or whatever I rename it)
// as a memeber and operates on it, or caches a decompressed version of the
// stream. Or we could parse the object and use it to fill in the values for
// a specific xref type and discard the indirect obect.

std::ostream &operator<<(std::ostream &os, const util::PdfObj &obj);
std::ostream &operator<<(std::ostream &os, const util::PdfObj *const obj);

// Parser Prototypes //////////////////////////////////////////////////////////

// NOTE the functions below are just prototypes and should go in the parser
// class once it is proper. I don't really know enough about the PDF format
// and how I would want to parse a whole document yet to build a nice parser
// object. This way I can experiement and change the way things are done
// without worrying about architecture and then work on putting them into
// a nice class with a good interface later.

// parse any object and return a pointer to it
// it will likely be necessary to have other finer grained functions to get
// specific object types where necessary
PdfObj *parse_pdf_obj(std::istream &is);

PdfArray *parse_pdf_array(std::istream &is);
PdfDict *parse_pdf_dict(std::istream &is);
PdfObj *parse_pdf_int_or_real(std::istream &is);
PdfObj *parse_pdf_num_ref_or_top_level(std::istream &is);

PdfName parse_pdf_name_obj(std::istream &is);
std::string get_name_token(std::istream &is);
bool parse_int(std::istream &is, int64_t *i);
bool parse_double(std::istream &is, double *d);
std::vector<char> parse_pdf_content_stream(std::istream &is);

// Parser helpers
void skip_whitespace(std::istream &is);
bool ends_name(char ch);
bool valid_name_char(char ch);

} // namespace util

#endif
