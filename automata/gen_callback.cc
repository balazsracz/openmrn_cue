
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <string>

using std::string;

void StringAppendF(string* s, const char* fmt, ...)
    __attribute__ ((format (printf, 2, 3)));


void StringAppendF(string* s, const char* fmt, ...) {
  int n;
  char buf[1000];
  const size_t size = sizeof(buf);
  va_list ap;

  va_start(ap, fmt);
  n = vsnprintf(buf, size, fmt, ap);
  va_end(ap);

  if (n > -1 && n < static_cast<int>(size)) {
    s->append(buf);
  } else {
    fprintf(stderr, "Buffer overrun in stringappendf.\n");
    abort();
  }
}


class CbTemplate {
 public:
  CbTemplate(int unbound_arg_count, int bound_arg_count,
             bool has_object, bool has_return)
      : unbound_arg_count_(unbound_arg_count),
        bound_arg_count_(bound_arg_count),
        has_object_(has_object),
        has_return_(has_return) {}

  string RenderVirtualBase() {
    string ret;
    string templates = RenderTemplateArgs(
        true, false, has_return_, 0, unbound_arg_count_);
    if (!templates.empty()) {
      ret += "template" + templates + " ";
    }
    ret += "class ";
    ret += VirtualBaseName();
    ret += " {\n public:\n  virtual ~" + VirtualBaseName() + "() {}\n";
    ret += "  virtual " + ReturnType() + " Run" +  + "(";
    ret += RenderArgList(false, false, 0, unbound_arg_count_);
    ret += ") = 0;\n";
    ret += "};\n";
    return ret;
  }

  string RenderSpecialization() {
    string ret;
    string templates = RenderTemplateArgs(
        true, has_object_, has_return_, bound_arg_count_, unbound_arg_count_);
    if (!templates.empty()) {
      ret += "template" + templates + " ";
    }
    ret += "class ";
    ret += SpecName();
    ret += " : public " + VirtualBaseName();
    ret += RenderTemplateArgs(
        false, false, has_return_, 0, unbound_arg_count_);
    // Destructor
    ret += " {\n public:\n  virtual ~" + SpecName() + "() {}\n";

    // Typedef for fptr_t

    ret += "  typedef " + FPtrType() + ";\n";


    // Constructor
    ret += "  " + SpecName() + "(";
    {
      bool first = true;
      if (has_object_) AddArg(&ret, &first, "T* object");
      AddArg(&ret, &first, "fptr_t fptr");
      for (int i = 1; i <= bound_arg_count_; i++) {
        AddArg(&ret, &first, ConstBoundArgRef(i));
      }
    }
    ret += ")\n      : fptr_(fptr)";
    if (has_object_) {
      ret += ", object_(object)";
    }
    {
      bool first = false;
      for (int i = 1; i <= bound_arg_count_; i++) {
        AddArg(&ret, &first, BoundArgVar(i) + "(" + BoundArgArg(i) + ")");
      }
    }
    ret += " {}\n";

    // Run method
    ret += "  virtual " + ReturnType() + " Run" +  + "(";
    ret += RenderArgList(false, false, 0, unbound_arg_count_);
    ret += ") {\n";
    ret += "    return (";
    if (has_object_) {
      ret += "object_->";
    }
    ret += "*fptr_)(";
    {
      string args;
      bool first = true;
      for (int i = 1; i <= bound_arg_count_; i++) {
        AddArg(&args, &first, BoundArgVar(i));
      }
      for (int i = 1; i <= unbound_arg_count_; i++) {
        AddArg(&args, &first, UnBoundArgArg(i));
      }
      ret += args;
    }
    ret += ");\n  }\n";


    // Variables
    ret += " private:\n";
    ret += "  fptr_t fptr_;\n";
    if (has_object_) ret += "  T* object_;\n";
    for (int i = 1; i <= bound_arg_count_; i++) {
      ret += "  typename std::remove_reference<" + BoundArgTemplate(i) + ">::type ";
      ret += BoundArgVar(i) + ";\n";
    }

    ret += "};\n";

    // NewCallback function
    if (has_object_) templates.insert(1,string("class U, "));
    for (bool ptr : {false, true}) {
      if (!templates.empty()) {
        ret += "template" + templates + " ";
      }
      string retclass =
          SpecName() +
          RenderTemplateArgs(false, has_object_, has_return_,
                             bound_arg_count_, unbound_arg_count_);
      ret += "inline " + retclass;
      if (ptr) ret += "* NewCallbackPtr("; else  ret += " NewCallback(";
      {
        bool first = true;
        if (has_object_) AddArg(&ret, &first, "U* obj");
        AddArg(&ret, &first, FPtrType());
        for (int i = 1; i <= bound_arg_count_; i++) {
          AddArg(&ret, &first, ConstBoundArgRef(i));
        }
      }
      ret += ") {\n  return " + (ptr?string("new ") : string())
          + retclass + "(";
      {
        bool first = true;
        if (has_object_) AddArg(&ret, &first, "obj");
        AddArg(&ret, &first, "fptr_t");
        for (int i = 1; i <= bound_arg_count_; i++) {
          AddArg(&ret, &first, BoundArgArg(i));
        }
      }
      ret += ");\n}\n";
    }

    return ret;
  };

  string FPtrType() {
    string ret = ReturnType() + " (";
    if (has_object_) ret += "T::";
    ret += "*fptr_t)(";
    ret += RenderArgList(false, false, bound_arg_count_, unbound_arg_count_);
    ret += ")";
    return ret;
  }

  string ReturnType() {
    if (has_return_) return "R"; else return "void";
  }

  string VirtualBaseName() {
    string ret;
    if (has_return_) ret += "R";
    ret += "Callback";
    if (unbound_arg_count_) StringAppendF(&ret, "%d", unbound_arg_count_);
    return ret;
  }

  string SpecName() {
    string ret;
    if (has_return_) ret += "R";
    if (has_object_) ret += "M";
    ret += "CallbackSpec";
    if (unbound_arg_count_ + bound_arg_count_) {
      StringAppendF(&ret, "%d_%d", unbound_arg_count_, bound_arg_count_);
    }
    return ret;
  }


  static string BoundArgTemplate(int n) {
    string ret;
    StringAppendF(&ret, "B%d", n);
    return ret;
  }

  static string UnBoundArgTemplate(int n) {
    string ret;
    StringAppendF(&ret, "P%d", n);
    return ret;
  }

  static string ConstBoundArgRef(int i) {
    string arg;
    StringAppendF(&arg,
                  "typename std::add_lvalue_reference<typename std::add_const<%s>::type>"
                  "::type %s", BoundArgTemplate(i).c_str(),
                  BoundArgArg(i).c_str());
    return arg;
  }

  static string BoundArgArg(int n) {
   string ret;
    StringAppendF(&ret, "b%d", n);
    return ret;
  }

  static string UnBoundArgArg(int n) {
    string ret;
    StringAppendF(&ret, "p%d", n);
    return ret;
  }

  static string BoundArgVar(int n) {
    return BoundArgArg(n) + "_";
  }

  static string UnBoundArgVar(int n) {
    return UnBoundArgArg(n) + "_";
  }

  static void AddArg(string* out, bool* first, const string& arg) {
    if (*first) {
      *first = false;
    } else {
      out->append(", ");
    }
    out->append(arg);
  }

  static void AddTemplateArg(string* out, bool* first, bool with_class,
                             const string& arg) {
    if (with_class) {
      AddArg(out, first, string("class ") + arg);
    } else {
      AddArg(out, first, arg);
    }
  }

  static string RenderTemplateArgs(bool with_class,
                                   bool has_object, bool has_return,
                                   int bound_count, int unbound_count) {
    string ret = "<";
    bool first = true;
    if (has_return) AddTemplateArg(&ret, &first, with_class, "R");
    if (has_object) AddTemplateArg(&ret, &first, with_class, "T");
    for (int i = 1; i <= bound_count; i++) {
      AddTemplateArg(&ret, &first, with_class, BoundArgTemplate(i));
    }
    for (int i = 1; i <= unbound_count; i++) {
      AddTemplateArg(&ret, &first, with_class, UnBoundArgTemplate(i));
    }
    if (first) ret = ""; else ret += ">";
    return ret;
  }

  static string RenderArgList(bool has_object, bool has_fn,
                              int bound_count, int unbound_count) {
    string ret;
    bool first = true;
    for (int i = 1; i <= bound_count; i++) {
      string arg;
      StringAppendF(&arg, "%s %s", BoundArgTemplate(i).c_str(),
                    BoundArgArg(i).c_str());
      AddArg(&ret, &first, arg);
    }
    for (int i = 1; i <= unbound_count; i++) {
      string arg;
      StringAppendF(&arg, "%s %s", UnBoundArgTemplate(i).c_str(),
                    UnBoundArgArg(i).c_str());
      AddArg(&ret, &first, arg);
    }
    return ret;
  }


 protected:
  int unbound_arg_count_;
  int bound_arg_count_;
  bool has_object_;
  bool has_return_;
};





int main(int argc, char*argv[]) {
  string s;
  printf("// Generated file. Do not edit.\n");
  printf("#include <type_traits>\n\n");
  for (bool has_ret : {false}) {
    for (int unb = 0; unb <= 2; unb++) {
      CbTemplate t(unb, 0, false, has_ret);
      printf("\n%s\n\n", t.RenderVirtualBase().c_str());

      for (bool has_object : {false, true}) {
        for (int bnd = 0; bnd <= 2; bnd++) {
          CbTemplate b(unb, bnd, has_object, has_ret);
          printf("%s\n", b.RenderSpecialization().c_str());
        }
      }
    }
  }
}
