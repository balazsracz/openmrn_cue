// Generated file. Do not edit.
#include <type_traits>


class Callback {
 public:
  virtual ~Callback() {}
  virtual void Run() = 0;
};


class CallbackSpec : public Callback {
 public:
  virtual ~CallbackSpec() {}
  typedef void (*fptr_t)();
  CallbackSpec(fptr_t fptr)
      : fptr_(fptr) {}
  virtual void Run() {
    return (*fptr_)();
  }
 private:
  fptr_t fptr_;
};
inline CallbackSpec NewCallback(void (*fptr_t)()) {
  return CallbackSpec(fptr_t);
}
inline CallbackSpec* NewCallbackPtr(void (*fptr_t)()) {
  return new CallbackSpec(fptr_t);
}

template<class B1> class CallbackSpec0_1 : public Callback {
 public:
  virtual ~CallbackSpec0_1() {}
  typedef void (*fptr_t)(B1 b1);
  CallbackSpec0_1(fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1)
      : fptr_(fptr), b1_(b1) {}
  virtual void Run() {
    return (*fptr_)(b1_);
  }
 private:
  fptr_t fptr_;
  typename std::remove_reference<B1>::type b1_;
};
template<class B1> inline CallbackSpec0_1<B1> NewCallback(void (*fptr_t)(B1 b1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return CallbackSpec0_1<B1>(fptr_t, b1);
}
template<class B1> inline CallbackSpec0_1<B1>* NewCallbackPtr(void (*fptr_t)(B1 b1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return new CallbackSpec0_1<B1>(fptr_t, b1);
}

template<class B1, class B2> class CallbackSpec0_2 : public Callback {
 public:
  virtual ~CallbackSpec0_2() {}
  typedef void (*fptr_t)(B1 b1, B2 b2);
  CallbackSpec0_2(fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2)
      : fptr_(fptr), b1_(b1), b2_(b2) {}
  virtual void Run() {
    return (*fptr_)(b1_, b2_);
  }
 private:
  fptr_t fptr_;
  typename std::remove_reference<B1>::type b1_;
  typename std::remove_reference<B2>::type b2_;
};
template<class B1, class B2> inline CallbackSpec0_2<B1, B2> NewCallback(void (*fptr_t)(B1 b1, B2 b2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return CallbackSpec0_2<B1, B2>(fptr_t, b1, b2);
}
template<class B1, class B2> inline CallbackSpec0_2<B1, B2>* NewCallbackPtr(void (*fptr_t)(B1 b1, B2 b2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return new CallbackSpec0_2<B1, B2>(fptr_t, b1, b2);
}

template<class T> class MCallbackSpec : public Callback {
 public:
  virtual ~MCallbackSpec() {}
  typedef void (T::*fptr_t)();
  MCallbackSpec(T* object, fptr_t fptr)
      : fptr_(fptr), object_(object) {}
  virtual void Run() {
    return (object_->*fptr_)();
  }
 private:
  fptr_t fptr_;
  T* object_;
};
template<class U, class T> inline MCallbackSpec<T> NewCallback(U* obj, void (T::*fptr_t)()) {
  return MCallbackSpec<T>(obj, fptr_t);
}
template<class U, class T> inline MCallbackSpec<T>* NewCallbackPtr(U* obj, void (T::*fptr_t)()) {
  return new MCallbackSpec<T>(obj, fptr_t);
}

template<class T, class B1> class MCallbackSpec0_1 : public Callback {
 public:
  virtual ~MCallbackSpec0_1() {}
  typedef void (T::*fptr_t)(B1 b1);
  MCallbackSpec0_1(T* object, fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1)
      : fptr_(fptr), object_(object), b1_(b1) {}
  virtual void Run() {
    return (object_->*fptr_)(b1_);
  }
 private:
  fptr_t fptr_;
  T* object_;
  typename std::remove_reference<B1>::type b1_;
};
template<class U, class T, class B1> inline MCallbackSpec0_1<T, B1> NewCallback(U* obj, void (T::*fptr_t)(B1 b1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return MCallbackSpec0_1<T, B1>(obj, fptr_t, b1);
}
template<class U, class T, class B1> inline MCallbackSpec0_1<T, B1>* NewCallbackPtr(U* obj, void (T::*fptr_t)(B1 b1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return new MCallbackSpec0_1<T, B1>(obj, fptr_t, b1);
}

template<class T, class B1, class B2> class MCallbackSpec0_2 : public Callback {
 public:
  virtual ~MCallbackSpec0_2() {}
  typedef void (T::*fptr_t)(B1 b1, B2 b2);
  MCallbackSpec0_2(T* object, fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2)
      : fptr_(fptr), object_(object), b1_(b1), b2_(b2) {}
  virtual void Run() {
    return (object_->*fptr_)(b1_, b2_);
  }
 private:
  fptr_t fptr_;
  T* object_;
  typename std::remove_reference<B1>::type b1_;
  typename std::remove_reference<B2>::type b2_;
};
template<class U, class T, class B1, class B2> inline MCallbackSpec0_2<T, B1, B2> NewCallback(U* obj, void (T::*fptr_t)(B1 b1, B2 b2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return MCallbackSpec0_2<T, B1, B2>(obj, fptr_t, b1, b2);
}
template<class U, class T, class B1, class B2> inline MCallbackSpec0_2<T, B1, B2>* NewCallbackPtr(U* obj, void (T::*fptr_t)(B1 b1, B2 b2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return new MCallbackSpec0_2<T, B1, B2>(obj, fptr_t, b1, b2);
}


template<class P1> class Callback1 {
 public:
  virtual ~Callback1() {}
  virtual void Run(P1 p1) = 0;
};


template<class P1> class CallbackSpec1_0 : public Callback1<P1> {
 public:
  virtual ~CallbackSpec1_0() {}
  typedef void (*fptr_t)(P1 p1);
  CallbackSpec1_0(fptr_t fptr)
      : fptr_(fptr) {}
  virtual void Run(P1 p1) {
    return (*fptr_)(p1);
  }
 private:
  fptr_t fptr_;
};
template<class P1> inline CallbackSpec1_0<P1> NewCallback(void (*fptr_t)(P1 p1)) {
  return CallbackSpec1_0<P1>(fptr_t);
}
template<class P1> inline CallbackSpec1_0<P1>* NewCallbackPtr(void (*fptr_t)(P1 p1)) {
  return new CallbackSpec1_0<P1>(fptr_t);
}

template<class B1, class P1> class CallbackSpec1_1 : public Callback1<P1> {
 public:
  virtual ~CallbackSpec1_1() {}
  typedef void (*fptr_t)(B1 b1, P1 p1);
  CallbackSpec1_1(fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1)
      : fptr_(fptr), b1_(b1) {}
  virtual void Run(P1 p1) {
    return (*fptr_)(b1_, p1);
  }
 private:
  fptr_t fptr_;
  typename std::remove_reference<B1>::type b1_;
};
template<class B1, class P1> inline CallbackSpec1_1<B1, P1> NewCallback(void (*fptr_t)(B1 b1, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return CallbackSpec1_1<B1, P1>(fptr_t, b1);
}
template<class B1, class P1> inline CallbackSpec1_1<B1, P1>* NewCallbackPtr(void (*fptr_t)(B1 b1, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return new CallbackSpec1_1<B1, P1>(fptr_t, b1);
}

template<class B1, class B2, class P1> class CallbackSpec1_2 : public Callback1<P1> {
 public:
  virtual ~CallbackSpec1_2() {}
  typedef void (*fptr_t)(B1 b1, B2 b2, P1 p1);
  CallbackSpec1_2(fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2)
      : fptr_(fptr), b1_(b1), b2_(b2) {}
  virtual void Run(P1 p1) {
    return (*fptr_)(b1_, b2_, p1);
  }
 private:
  fptr_t fptr_;
  typename std::remove_reference<B1>::type b1_;
  typename std::remove_reference<B2>::type b2_;
};
template<class B1, class B2, class P1> inline CallbackSpec1_2<B1, B2, P1> NewCallback(void (*fptr_t)(B1 b1, B2 b2, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return CallbackSpec1_2<B1, B2, P1>(fptr_t, b1, b2);
}
template<class B1, class B2, class P1> inline CallbackSpec1_2<B1, B2, P1>* NewCallbackPtr(void (*fptr_t)(B1 b1, B2 b2, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return new CallbackSpec1_2<B1, B2, P1>(fptr_t, b1, b2);
}

template<class T, class P1> class MCallbackSpec1_0 : public Callback1<P1> {
 public:
  virtual ~MCallbackSpec1_0() {}
  typedef void (T::*fptr_t)(P1 p1);
  MCallbackSpec1_0(T* object, fptr_t fptr)
      : fptr_(fptr), object_(object) {}
  virtual void Run(P1 p1) {
    return (object_->*fptr_)(p1);
  }
 private:
  fptr_t fptr_;
  T* object_;
};
template<class U, class T, class P1> inline MCallbackSpec1_0<T, P1> NewCallback(U* obj, void (T::*fptr_t)(P1 p1)) {
  return MCallbackSpec1_0<T, P1>(obj, fptr_t);
}
template<class U, class T, class P1> inline MCallbackSpec1_0<T, P1>* NewCallbackPtr(U* obj, void (T::*fptr_t)(P1 p1)) {
  return new MCallbackSpec1_0<T, P1>(obj, fptr_t);
}

template<class T, class B1, class P1> class MCallbackSpec1_1 : public Callback1<P1> {
 public:
  virtual ~MCallbackSpec1_1() {}
  typedef void (T::*fptr_t)(B1 b1, P1 p1);
  MCallbackSpec1_1(T* object, fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1)
      : fptr_(fptr), object_(object), b1_(b1) {}
  virtual void Run(P1 p1) {
    return (object_->*fptr_)(b1_, p1);
  }
 private:
  fptr_t fptr_;
  T* object_;
  typename std::remove_reference<B1>::type b1_;
};
template<class U, class T, class B1, class P1> inline MCallbackSpec1_1<T, B1, P1> NewCallback(U* obj, void (T::*fptr_t)(B1 b1, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return MCallbackSpec1_1<T, B1, P1>(obj, fptr_t, b1);
}
template<class U, class T, class B1, class P1> inline MCallbackSpec1_1<T, B1, P1>* NewCallbackPtr(U* obj, void (T::*fptr_t)(B1 b1, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return new MCallbackSpec1_1<T, B1, P1>(obj, fptr_t, b1);
}

template<class T, class B1, class B2, class P1> class MCallbackSpec1_2 : public Callback1<P1> {
 public:
  virtual ~MCallbackSpec1_2() {}
  typedef void (T::*fptr_t)(B1 b1, B2 b2, P1 p1);
  MCallbackSpec1_2(T* object, fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2)
      : fptr_(fptr), object_(object), b1_(b1), b2_(b2) {}
  virtual void Run(P1 p1) {
    return (object_->*fptr_)(b1_, b2_, p1);
  }
 private:
  fptr_t fptr_;
  T* object_;
  typename std::remove_reference<B1>::type b1_;
  typename std::remove_reference<B2>::type b2_;
};
template<class U, class T, class B1, class B2, class P1> inline MCallbackSpec1_2<T, B1, B2, P1> NewCallback(U* obj, void (T::*fptr_t)(B1 b1, B2 b2, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return MCallbackSpec1_2<T, B1, B2, P1>(obj, fptr_t, b1, b2);
}
template<class U, class T, class B1, class B2, class P1> inline MCallbackSpec1_2<T, B1, B2, P1>* NewCallbackPtr(U* obj, void (T::*fptr_t)(B1 b1, B2 b2, P1 p1), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return new MCallbackSpec1_2<T, B1, B2, P1>(obj, fptr_t, b1, b2);
}


template<class P1, class P2> class Callback2 {
 public:
  virtual ~Callback2() {}
  virtual void Run(P1 p1, P2 p2) = 0;
};


template<class P1, class P2> class CallbackSpec2_0 : public Callback2<P1, P2> {
 public:
  virtual ~CallbackSpec2_0() {}
  typedef void (*fptr_t)(P1 p1, P2 p2);
  CallbackSpec2_0(fptr_t fptr)
      : fptr_(fptr) {}
  virtual void Run(P1 p1, P2 p2) {
    return (*fptr_)(p1, p2);
  }
 private:
  fptr_t fptr_;
};
template<class P1, class P2> inline CallbackSpec2_0<P1, P2> NewCallback(void (*fptr_t)(P1 p1, P2 p2)) {
  return CallbackSpec2_0<P1, P2>(fptr_t);
}
template<class P1, class P2> inline CallbackSpec2_0<P1, P2>* NewCallbackPtr(void (*fptr_t)(P1 p1, P2 p2)) {
  return new CallbackSpec2_0<P1, P2>(fptr_t);
}

template<class B1, class P1, class P2> class CallbackSpec2_1 : public Callback2<P1, P2> {
 public:
  virtual ~CallbackSpec2_1() {}
  typedef void (*fptr_t)(B1 b1, P1 p1, P2 p2);
  CallbackSpec2_1(fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1)
      : fptr_(fptr), b1_(b1) {}
  virtual void Run(P1 p1, P2 p2) {
    return (*fptr_)(b1_, p1, p2);
  }
 private:
  fptr_t fptr_;
  typename std::remove_reference<B1>::type b1_;
};
template<class B1, class P1, class P2> inline CallbackSpec2_1<B1, P1, P2> NewCallback(void (*fptr_t)(B1 b1, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return CallbackSpec2_1<B1, P1, P2>(fptr_t, b1);
}
template<class B1, class P1, class P2> inline CallbackSpec2_1<B1, P1, P2>* NewCallbackPtr(void (*fptr_t)(B1 b1, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return new CallbackSpec2_1<B1, P1, P2>(fptr_t, b1);
}

template<class B1, class B2, class P1, class P2> class CallbackSpec2_2 : public Callback2<P1, P2> {
 public:
  virtual ~CallbackSpec2_2() {}
  typedef void (*fptr_t)(B1 b1, B2 b2, P1 p1, P2 p2);
  CallbackSpec2_2(fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2)
      : fptr_(fptr), b1_(b1), b2_(b2) {}
  virtual void Run(P1 p1, P2 p2) {
    return (*fptr_)(b1_, b2_, p1, p2);
  }
 private:
  fptr_t fptr_;
  typename std::remove_reference<B1>::type b1_;
  typename std::remove_reference<B2>::type b2_;
};
template<class B1, class B2, class P1, class P2> inline CallbackSpec2_2<B1, B2, P1, P2> NewCallback(void (*fptr_t)(B1 b1, B2 b2, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return CallbackSpec2_2<B1, B2, P1, P2>(fptr_t, b1, b2);
}
template<class B1, class B2, class P1, class P2> inline CallbackSpec2_2<B1, B2, P1, P2>* NewCallbackPtr(void (*fptr_t)(B1 b1, B2 b2, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return new CallbackSpec2_2<B1, B2, P1, P2>(fptr_t, b1, b2);
}

template<class T, class P1, class P2> class MCallbackSpec2_0 : public Callback2<P1, P2> {
 public:
  virtual ~MCallbackSpec2_0() {}
  typedef void (T::*fptr_t)(P1 p1, P2 p2);
  MCallbackSpec2_0(T* object, fptr_t fptr)
      : fptr_(fptr), object_(object) {}
  virtual void Run(P1 p1, P2 p2) {
    return (object_->*fptr_)(p1, p2);
  }
 private:
  fptr_t fptr_;
  T* object_;
};
template<class U, class T, class P1, class P2> inline MCallbackSpec2_0<T, P1, P2> NewCallback(U* obj, void (T::*fptr_t)(P1 p1, P2 p2)) {
  return MCallbackSpec2_0<T, P1, P2>(obj, fptr_t);
}
template<class U, class T, class P1, class P2> inline MCallbackSpec2_0<T, P1, P2>* NewCallbackPtr(U* obj, void (T::*fptr_t)(P1 p1, P2 p2)) {
  return new MCallbackSpec2_0<T, P1, P2>(obj, fptr_t);
}

template<class T, class B1, class P1, class P2> class MCallbackSpec2_1 : public Callback2<P1, P2> {
 public:
  virtual ~MCallbackSpec2_1() {}
  typedef void (T::*fptr_t)(B1 b1, P1 p1, P2 p2);
  MCallbackSpec2_1(T* object, fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1)
      : fptr_(fptr), object_(object), b1_(b1) {}
  virtual void Run(P1 p1, P2 p2) {
    return (object_->*fptr_)(b1_, p1, p2);
  }
 private:
  fptr_t fptr_;
  T* object_;
  typename std::remove_reference<B1>::type b1_;
};
template<class U, class T, class B1, class P1, class P2> inline MCallbackSpec2_1<T, B1, P1, P2> NewCallback(U* obj, void (T::*fptr_t)(B1 b1, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return MCallbackSpec2_1<T, B1, P1, P2>(obj, fptr_t, b1);
}
template<class U, class T, class B1, class P1, class P2> inline MCallbackSpec2_1<T, B1, P1, P2>* NewCallbackPtr(U* obj, void (T::*fptr_t)(B1 b1, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1) {
  return new MCallbackSpec2_1<T, B1, P1, P2>(obj, fptr_t, b1);
}

template<class T, class B1, class B2, class P1, class P2> class MCallbackSpec2_2 : public Callback2<P1, P2> {
 public:
  virtual ~MCallbackSpec2_2() {}
  typedef void (T::*fptr_t)(B1 b1, B2 b2, P1 p1, P2 p2);
  MCallbackSpec2_2(T* object, fptr_t fptr, typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2)
      : fptr_(fptr), object_(object), b1_(b1), b2_(b2) {}
  virtual void Run(P1 p1, P2 p2) {
    return (object_->*fptr_)(b1_, b2_, p1, p2);
  }
 private:
  fptr_t fptr_;
  T* object_;
  typename std::remove_reference<B1>::type b1_;
  typename std::remove_reference<B2>::type b2_;
};
template<class U, class T, class B1, class B2, class P1, class P2> inline MCallbackSpec2_2<T, B1, B2, P1, P2> NewCallback(U* obj, void (T::*fptr_t)(B1 b1, B2 b2, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return MCallbackSpec2_2<T, B1, B2, P1, P2>(obj, fptr_t, b1, b2);
}
template<class U, class T, class B1, class B2, class P1, class P2> inline MCallbackSpec2_2<T, B1, B2, P1, P2>* NewCallbackPtr(U* obj, void (T::*fptr_t)(B1 b1, B2 b2, P1 p1, P2 p2), typename std::add_lvalue_reference<typename std::add_const<B1>::type>::type b1, typename std::add_lvalue_reference<typename std::add_const<B2>::type>::type b2) {
  return new MCallbackSpec2_2<T, B1, B2, P1, P2>(obj, fptr_t, b1, b2);
}

