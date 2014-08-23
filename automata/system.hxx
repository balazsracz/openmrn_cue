#ifndef _bracz_train_automata_system_hxx_
#define _bracz_train_automata_system_hxx_

#include <stdio.h>
#include <assert.h>

#include <string>
#include <vector>
#include <map>
#include <set>

#include "../cs/src/automata_defs.h"

using std::string;
using std::vector;
using std::map;
using std::set;

#ifndef HASSERT
#define HASSERT(x) do { if (!(x)) {fprintf(stderr, "Assertion failed: " #x); abort();} } while(0)
#endif

namespace automata {

string StringPrintf(const char* format, ...);

#ifndef DISALLOW_COPY_AND_ASSIGN
//! Put this into the private section of a class to prevent the default copy
//! constructors to be generated.
#define DISALLOW_COPY_AND_ASSIGN(Name) Name& operator=(Name&); Name(Name&)
#endif

class Automata;
class GlobalVariable;

/** This class represents an entire board and contains all the bit definitions
    and all automatas that this board contains. This class is able to
    eventually output a bytestream for the compiled automata.
 */
class Board {
public:
    Board() {}
    ~Board();

    //! Generates the binary data for the entire board.
    void Render(string* output);

    void AddAutomata(Automata* a) {
        automatas_.push_back(AutomataInfo(*a));
    }

    void AddVariable(GlobalVariable* v) {
        global_variables_.push_back(v);
    }

private:
    void RenderPreamble(string* output);
    void RenderAutomatas(string* output);

    struct AutomataInfo {
        AutomataInfo(Automata& a)
            : automata(&a), ptr_offset(-1), offset(-1) {}
        ~AutomataInfo() { }
        Automata* automata;
        int ptr_offset; //< Offset in the binary to the pointer to the automata.
        int offset; //< This is the code's offset in the binary.
    };

    vector<AutomataInfo> automatas_;
    vector<GlobalVariable*> global_variables_;

    DISALLOW_COPY_AND_ASSIGN(Board);
};

struct GlobalVariableId {
public:
    GlobalVariableId() : id(0), arg(0) {}
    uint32_t id;
    uint16_t arg;
};

/** Represents a single bit that the automata runner system will track. This
    variable is not assigned to any automata and will create an entry in the
    preamble.
*/
class GlobalVariable {
public:
    GlobalVariable()
        : id_assigned_(false) {}
    virtual ~GlobalVariable() {}
    virtual GlobalVariableId GetId() const {
        assert(id_assigned_);
        return id_;
    }
    void SetId(uint32_t id) {
        id_assigned_ = true;
        id_.id = id;
    }
    void SetArg(uint16_t arg) {
        id_.arg = arg;
    }
    //! Appends the rendered version of this variable to the preamble of the
    //! board code.
    virtual void Render(string* output) = 0;

    bool HasId() { return id_assigned_; }

    // These methods are used by the unittesting frameworks to declare a
    // listener.
    virtual uint64_t event_on() const = 0;
    virtual uint64_t event_off() const = 0;

protected:
    bool id_assigned_;
    GlobalVariableId id_;
    DISALLOW_COPY_AND_ASSIGN(GlobalVariable);
};

extern int FIRST_USER_STATE_ID;

/** Represents a concrete automata in the current board.
 */
class Automata {
 public:
  Automata(const string& name)
      : timer_bit_(0),
        output_(NULL),
        aut(this),
        name_(name),
        next_user_state_(FIRST_USER_STATE_ID) {
    ClearUsedVariables();
  }
  Automata()
      : timer_bit_(0),
        output_(NULL),
        aut(this),
        next_user_state_(FIRST_USER_STATE_ID) {
    ClearUsedVariables();
  }
    ~Automata() {}

    void Render(string* output);

    string* output() { return output_; }

    class Op;

    struct LocalVariable {
    public:
        LocalVariable(): id(-1) {}
        explicit LocalVariable(int iid): id(iid) {}
        int GetId() const {
            assert(id >= 0 && id < 32);
            return id;
        }
        int id;
        // We purposefully allow this object to be copied.
    };

    // Adds a global variable reference to the used local variables, and
    // returns the local variable reference. The local variable reference is
    // owned by the Automata object. The same global var can be imported
    // multiple times (and will get the same ID).
    LocalVariable* ImportVariable(GlobalVariable* var);
    const LocalVariable& ImportVariable(const GlobalVariable& var);
    // Reserves a local variable that will need to be overridden by
    // ActImportvariable. The return is an rvalue to prevent it being assigned
    // to a reference.
    LocalVariable ReserveVariable();

    const LocalVariable& timer_bit() { return timer_bit_; }

    int NewUserState() {
      int id = next_user_state_++;
      HASSERT(!(id & _IF_STATE_MASK));
      return id;
    }

    void DefCopy(const LocalVariable& src, LocalVariable* dst);
    void DefNCopy(const LocalVariable& src, LocalVariable* dst);

    void ClearUsedVariables();

    void RenderImportVariable(const GlobalVariable& var, int local_id);

protected:
    virtual void Body() = 0;

    LocalVariable timer_bit_;

#define Def() Automata::Op(aut, aut->output())

    string* output_;

    Automata* const aut;

    virtual Board* board() = 0;

private:
    friend class Op;

    // Returns the next available variable ID, skipping any reserved ids.
    int GetNextVariableId();

    map<const GlobalVariable*, LocalVariable> used_variables_;
    set<int> reserved_variables_;
    int next_variable_id_;
    string name_;
    int next_user_state_;
    DISALLOW_COPY_AND_ASSIGN(Automata);
};

#define DefCustomAut(name, boardref, base, body...) class Aut##name : public base {public:  Aut##name(decltype(boardref)* board) : base(#name), board_(board) {board_->AddAutomata(this); } protected: decltype(boardref)* board_; virtual Board* board() {return board_;} virtual void Body() body  } name##instance(&(boardref))

#define DefAut(name, boardref, body...) DefCustomAut(name, boardref, automata::Automata, body)

}

#endif // _bracz_train_automata_system_hxx_
