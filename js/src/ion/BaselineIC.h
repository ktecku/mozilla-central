/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_ic_h__) && defined(JS_ION)
#define jsion_baseline_ic_h__

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsopcode.h"
#include "BaselineJIT.h"
#include "BaselineRegisters.h"

#include "gc/Heap.h"

namespace js {
namespace ion {

//
// Baseline Inline Caches are polymorphic caches that aggressively
// share their stub code.
//
// Every polymorphic site contains a linked list of stubs which are
// specific to that site.  These stubs are composed of a |StubData|
// structure that stores parametrization information (e.g.
// the shape pointer for a shape-check-and-property-get stub), any
// dynamic information (e.g. use counts), a pointer to the stub code,
// and a pointer to the next stub state in the linked list.
//
// Every BaselineScript keeps an table of |CacheDescriptor| data
// structures, which store the following:
//      A pointer to the first StubData in the cache.
//      The bytecode PC of the relevant IC.
//      The machine-code PC where the call to the stubcode returns.
//
// A diagram:
//
//        Control flow                  Pointers
//      =======#                     ----.     .---->
//             #                         |     |
//             #======>                  \-----/
//
//
//                                   .---------------------------------------.
//                                   |         .-------------------------.   |
//                                   |         |         .----.          |   |
//         Baseline                  |         |         |    |          |   |
//         JIT Code              0   ^     1   ^     2   ^    |          |   |
//     +--------------+    .-->+-----+   +-----+   +-----+    |          |   |
//     |              |  #=|==>|     |==>|     |==>| FB  |    |          |   |
//     |              |  # |   +-----+   +-----+   +-----+    |          |   |
//     |              |  # |      #         #         #       |          |   |
//     |==============|==# |      #         #         #       |          |   |
//     |=== IC =======|    |      #         #         #       |          |   |
//  .->|==============|<===|======#=========#=========#       |          |   |
//  |  |              |    |                                  |          |   |
//  |  |              |    |                                  |          |   |
//  |  |              |    |                                  |          |   |
//  |  |              |    |                                  v          |   |
//  |  |              |    |                              +---------+    |   |
//  |  |              |    |                              | Fallback|    |   |
//  |  |              |    |                              | Stub    |    |   |
//  |  |              |    |                              | Code    |    |   |
//  |  |              |    |                              +---------+    |   |
//  |  +--------------+    |                                             |   |
//  |         |_______     |                              +---------+    |   |
//  |                |     |                              | Stub    |<---/   |
//  |        IC      |     \--.                           | Code    |        |
//  |    Descriptor  |        |                           +---------+        |
//  |      Table     v        |                                              |
//  |  +-----------------+    |                           +---------+        |
//  \--| Ins | PC | Stub |----/                           | Stub    |<-------/
//     +-----------------+                                | Code    |
//     |       ...       |                                +---------+
//     +-----------------+
//                                                          Shared
//                                                          Stub Code
//
//
// Type ICs
// ========
//
// Type ICs are otherwise regular ICs that are actually nested within
// other IC chains.  They serve to optimize locations in the code where the
// baseline compiler would have otherwise had to perform a type Monitor operation
// (e.g. the result of GetProp, GetElem, etc.), or locations where the baseline
// compiler would have had to modify a heap typeset using the type of an input
// value (e.g. SetProp, SetElem, etc.)
//
// There are two kinds of Type ICs: Monitor and Update.
//
// Note that type stub bodies are no-ops.  The stubs only exist for their
// guards, and their existence simply signifies that the typeset (implicit)
// that is being checked already contains that type.
//
// TypeMonitor ICs
// ---------------
// Monitor ICs are shared between stubs in the general IC, and monitor the resulting
// types of getter operations (call returns, getprop outputs, etc.)
//
//        +-----------+     +-----------+     +-----------+     +-----------+
//   ---->| Stub 1    |---->| Stub 2    |---->| Stub 3    |---->| FB Stub   |
//        +-----------+     +-----------+     +-----------+     +-----------+
//             |                  |                 |                  |
//             |------------------/-----------------/                  |
//             v                                                       |
//        +-----------+     +-----------+     +-----------+            |
//        | Type 1    |---->| Type 2    |---->| Type FB   |            |
//        +-----------+     +-----------+     +-----------+            |
//             |                 |                  |                  |
//  <----------/-----------------/------------------/------------------/
//                r e t u r n    p a t h
//
// After an optimized IC stub successfully executes, it passes control to the type stub
// chain to check the resulting type.  If no type stub succeeds, and the monitor fallback
// stub is reached, the monitor fallback stub performs a manual monitor, and also adds the
// appropriate type stub to the chain.
//
// The IC's main fallback, in addition to generating new mainline stubs, also generates
// type stubs as reflected by its returned value.
//
// NOTE: The type IC chain returns directly to the mainline code, not back to the
// stub it was entered from.  Thus, entering a type IC is a matter of a |jump|, not
// a |call|.  This allows us to safely call a VM Monitor function from within the monitor IC's
// fallback chain, since the return address (needed for stack inspection) is preserved.
//
//
// TypeUpdate ICs
// --------------
// Update ICs update heap typesets and monitor the input types of setter operations
// (setelem, setprop inputs, etc.).  Unlike monitor ICs, they are not shared
// between stubs on an IC, but instead are kept track of on a per-stub basis.
//
// This is because the main stubs for the operation will each identify a potentially
// different TypeObject to update.  New input types must be tracked on a typeobject-to-
// typeobject basis.
//
// Type-update ICs cannot be called in tail position (they must return to the
// the stub that called them so that the stub may continue to perform its original
// purpose).  This means that any VMCall to perform a manual type update from C++ must be
// done from within the main IC stub.  This necessitates that the stub enter a
// "BaselineStub" frame before making the call.
//
// If the type-update IC chain could itself make the VMCall, then the BaselineStub frame
// must be entered before calling the type-update chain, and exited afterward.  This
// is very expensive for a common case where we expect the type-update fallback to not
// be called.  To avoid the cost of entering and exiting a BaselineStub frame when
// using the type-update IC chain, we design the chain to not perform any VM-calls
// in its fallback.
//
// Instead, the type-update IC chain is responsible for returning 1 or 0, depending
// on if a type is represented in the chain or not.  The fallback stub simply returns
// 0, and all other optimized stubs return 1.
// If the chain returns 1, then the IC stub goes ahead and performs its operation.
// If the chain returns 0, then the IC stub performs a call to the fallback function
// inline (doing the requisite BaselineStub frame enter/exit).
// This allows us to avoid the expensive subfram enter/exit in the common case.
//
//                                 r e t u r n    p a t h
//   <--------------.-----------------.-----------------.-----------------.
//                  |                 |                 |                 |
//        +-----------+     +-----------+     +-----------+     +-----------+
//   ---->| Stub 1    |---->| Stub 2    |---->| Stub 3    |---->| FB Stub   |
//        +-----------+     +-----------+     +-----------+     +-----------+
//          |   ^             |   ^             |   ^
//          |   |             |   |             |   |
//          |   |             |   |             |   |----------------.
//          |   |             |   |             v   |1               |0
//          |   |             |   |         +-----------+    +-----------+
//          |   |             |   |         | Type 3.1  |--->|    FB 3   |
//          |   |             |   |         +-----------+    +-----------+
//          |   |             |   |
//          |   |             |   \-------------.-----------------.
//          |   |             |   |             |                 |
//          |   |             v   |1            |1                |0
//          |   |         +-----------+     +-----------+     +-----------+
//          |   |         | Type 2.1  |---->| Type 2.2  |---->|    FB 2   |
//          |   |         +-----------+     +-----------+     +-----------+
//          |   |
//          |   \-------------.-----------------.
//          |   |             |                 |
//          v   |1            |1                |0
//     +-----------+     +-----------+     +-----------+
//     | Type 1.1  |---->| Type 1.2  |---->|   FB 1    |
//     +-----------+     +-----------+     +-----------+
//

class ICStub;
class ICFallbackStub;

//
// An entry in the Baseline IC descriptor table.
//
class ICEntry
{
  private:
    // Offset from the start of the JIT code where the IC
    // load and call instructions are.
    uint32_t returnOffset_;

    // The PC of this IC's bytecode op within the JSScript.
    uint32_t pcOffset_ : 31;

    // Whether this IC is for a bytecode op.
    uint32_t isForOp_ : 1;

    // A pointer to the baseline IC stub for this instruction.
    ICStub *firstStub_;

  public:
    ICEntry(uint32_t pcOffset, bool isForOp)
      : returnOffset_(), pcOffset_(pcOffset), isForOp_(isForOp), firstStub_(NULL)
    {}

    CodeOffsetLabel returnOffset() const {
        return CodeOffsetLabel(returnOffset_);
    }

    void setReturnOffset(CodeOffsetLabel offset) {
        JS_ASSERT(offset.offset() <= (size_t) UINT32_MAX);
        returnOffset_ = (uint32_t) offset.offset();
    }

    void fixupReturnOffset(MacroAssembler &masm) {
        CodeOffsetLabel offset = returnOffset();
        offset.fixup(&masm);
        JS_ASSERT(offset.offset() <= UINT32_MAX);
        returnOffset_ = (uint32_t) offset.offset();
    }

    uint32_t pcOffset() const {
        return pcOffset_;
    }

    jsbytecode *pc(JSScript *script) const {
        return script->code + pcOffset_;
    }

    bool isForOp() const {
        return isForOp_;
    }

    bool hasStub() const {
        return firstStub_ != NULL;
    }
    ICStub *firstStub() const {
        JS_ASSERT(hasStub());
        return firstStub_;
    }

    ICFallbackStub *fallbackStub() const;

    void setFirstStub(ICStub *stub) {
        firstStub_ = stub;
    }

    static inline size_t offsetOfFirstStub() {
        return offsetof(ICEntry, firstStub_);
    }

    inline ICStub **addressOfFirstStub() {
        return &firstStub_;
    }
};

// List of baseline IC stub kinds.
#define IC_STUB_KIND_LIST(_)    \
    _(StackCheck_Fallback)      \
                                \
    _(UseCount_Fallback)        \
                                \
    _(TypeMonitor_Fallback)     \
    _(TypeMonitor_Primitive)    \
    _(TypeMonitor_SingleObject) \
    _(TypeMonitor_TypeObject)   \
                                \
    _(TypeUpdate_Fallback)      \
    _(TypeUpdate_Primitive)     \
    _(TypeUpdate_SingleObject)  \
    _(TypeUpdate_TypeObject)    \
                                \
    _(This_Fallback)            \
                                \
    _(NewArray_Fallback)        \
    _(NewObject_Fallback)       \
                                \
    _(Compare_Fallback)         \
    _(Compare_Int32)            \
    _(Compare_Double)           \
    _(Compare_NumberWithUndefined) \
    _(Compare_String)           \
    _(Compare_Boolean)          \
    _(Compare_Object)           \
    _(Compare_ObjectWithUndefined) \
                                \
    _(ToBool_Fallback)          \
    _(ToBool_Int32)             \
    _(ToBool_String)            \
    _(ToBool_NullUndefined)     \
                                \
    _(ToNumber_Fallback)        \
                                \
    _(BinaryArith_Fallback)     \
    _(BinaryArith_Int32)        \
    _(BinaryArith_Double)       \
    _(BinaryArith_StringConcat) \
    _(BinaryArith_StringObjectConcat) \
                                \
    _(UnaryArith_Fallback)      \
    _(UnaryArith_Int32)         \
    _(UnaryArith_Double)        \
                                \
    _(Call_Fallback)            \
    _(Call_Scripted)            \
    _(Call_Native)              \
                                \
    _(GetElem_Fallback)         \
    _(GetElem_Native)           \
    _(GetElem_NativePrototype)  \
    _(GetElem_String)           \
    _(GetElem_Dense)            \
    _(GetElem_TypedArray)       \
                                \
    _(SetElem_Fallback)         \
    _(SetElem_Dense)            \
    _(SetElem_DenseAdd)         \
    _(SetElem_TypedArray)       \
                                \
    _(In_Fallback)              \
                                \
    _(GetName_Fallback)         \
    _(GetName_Global)           \
    _(GetName_Scope0)           \
    _(GetName_Scope1)           \
    _(GetName_Scope2)           \
    _(GetName_Scope3)           \
    _(GetName_Scope4)           \
                                \
    _(BindName_Fallback)        \
                                \
    _(GetIntrinsic_Fallback)    \
    _(GetIntrinsic_Constant)    \
                                \
    _(GetProp_Fallback)         \
    _(GetProp_ArrayLength)      \
    _(GetProp_TypedArrayLength) \
    _(GetProp_String)           \
    _(GetProp_StringLength)     \
    _(GetProp_Native)           \
    _(GetProp_NativePrototype)  \
                                \
    _(SetProp_Fallback)         \
    _(SetProp_Native)           \
                                \
    _(TableSwitch)              \
                                \
    _(IteratorNew_Fallback)     \
    _(IteratorMore_Fallback)    \
    _(IteratorMore_Native)      \
    _(IteratorNext_Fallback)    \
    _(IteratorNext_Native)      \
    _(IteratorClose_Fallback)   \
                                \
    _(InstanceOf_Fallback)      \
                                \
    _(TypeOf_Fallback)

#define FORWARD_DECLARE_STUBS(kindName) class IC##kindName;
    IC_STUB_KIND_LIST(FORWARD_DECLARE_STUBS)
#undef FORWARD_DECLARE_STUBS

class ICFallbackStub;
class ICMonitoredStub;
class ICMonitoredFallbackStub;
class ICUpdatedStub;

//
// Base class for all IC stubs.
//
class ICStub
{
    friend class ICFallbackStub;

  public:
    enum Kind {
        INVALID = 0,
#define DEF_ENUM_KIND(kindName) kindName,
        IC_STUB_KIND_LIST(DEF_ENUM_KIND)
#undef DEF_ENUM_KIND
        LIMIT
    };

    static inline bool IsValidKind(Kind k) {
        return (k > INVALID) && (k < LIMIT);
    }

    static const char *KindString(Kind k) {
        switch(k) {
#define DEF_KIND_STR(kindName) case kindName: return #kindName;
            IC_STUB_KIND_LIST(DEF_KIND_STR)
#undef DEF_KIND_STR
          default:
            JS_NOT_REACHED("Invalid kind.");
            return "INVALID_KIND";
        }
    }

    enum Trait {
        Regular             = 0x0,
        Fallback            = 0x1,
        Monitored           = 0x2,
        MonitoredFallback   = 0x3,
        Updated             = 0x4
    };

    void markCode(JSTracer *trc, const char *name);
    void trace(JSTracer *trc);

  protected:
    // The kind of the stub.
    //  High bit is 'isFallback' flag.
    //  Second high bit is 'isMonitored' flag.
    Trait trait_ : 3;
    Kind kind_ : 13;

    // A 16-bit field usable by subtypes of ICStub for subtype-specific small-info
    uint16_t extra_;

    // The raw jitcode to call for this stub.
    uint8_t *stubCode_;

    // Pointer to next IC stub.  This is null for the last IC stub, which should
    // either be a fallback or inert IC stub.
    ICStub *next_;

    inline ICStub(Kind kind, IonCode *stubCode)
      : trait_(Regular),
        kind_(kind),
        extra_(0),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline ICStub(Kind kind, Trait trait, IonCode *stubCode)
      : trait_(trait),
        kind_(kind),
        extra_(0),
        stubCode_(stubCode->raw()),
        next_(NULL)
    {
        JS_ASSERT(stubCode != NULL);
    }

    inline Trait trait() const {
        // Workaround for MSVC reading trait_ as signed value.
        return (Trait)(trait_ & 0x7);
    }

  public:

    inline Kind kind() const {
        return static_cast<Kind>(kind_);
    }

    inline bool isFallback() const {
        return trait() == Fallback || trait() == MonitoredFallback;
    }

    inline bool isMonitored() const {
        return trait() == Monitored;
    }

    inline bool isUpdated() const {
        return trait() == Updated;
    }

    inline bool isMonitoredFallback() const {
        return trait() == MonitoredFallback;
    }

    inline const ICFallbackStub *toFallbackStub() const {
        JS_ASSERT(isFallback());
        return reinterpret_cast<const ICFallbackStub *>(this);
    }

    inline ICFallbackStub *toFallbackStub() {
        JS_ASSERT(isFallback());
        return reinterpret_cast<ICFallbackStub *>(this);
    }

    inline const ICMonitoredStub *toMonitoredStub() const {
        JS_ASSERT(isMonitored());
        return reinterpret_cast<const ICMonitoredStub *>(this);
    }

    inline ICMonitoredStub *toMonitoredStub() {
        JS_ASSERT(isMonitored());
        return reinterpret_cast<ICMonitoredStub *>(this);
    }

    inline const ICMonitoredFallbackStub *toMonitoredFallbackStub() const {
        JS_ASSERT(isMonitoredFallback());
        return reinterpret_cast<const ICMonitoredFallbackStub *>(this);
    }

    inline ICMonitoredFallbackStub *toMonitoredFallbackStub() {
        JS_ASSERT(isMonitoredFallback());
        return reinterpret_cast<ICMonitoredFallbackStub *>(this);
    }

    inline const ICUpdatedStub *toUpdatedStub() const {
        JS_ASSERT(isUpdated());
        return reinterpret_cast<const ICUpdatedStub *>(this);
    }

    inline ICUpdatedStub *toUpdatedStub() {
        JS_ASSERT(isUpdated());
        return reinterpret_cast<ICUpdatedStub *>(this);
    }

#define KIND_METHODS(kindName)   \
    inline bool is##kindName() const { return kind() == kindName; } \
    inline const IC##kindName *to##kindName() const { \
        JS_ASSERT(is##kindName()); \
        return reinterpret_cast<const IC##kindName *>(this); \
    } \
    inline IC##kindName *to##kindName() { \
        JS_ASSERT(is##kindName()); \
        return reinterpret_cast<IC##kindName *>(this); \
    }
    IC_STUB_KIND_LIST(KIND_METHODS)
#undef KIND_METHODS

    inline ICStub *next() const {
        return next_;
    }

    inline bool hasNext() const {
        return next_ != NULL;
    }

    inline void setNext(ICStub *stub) {
        next_ = stub;
    }

    inline ICStub **addressOfNext() {
        return &next_;
    }

    inline IonCode *ionCode() {
        return IonCode::FromExecutable(stubCode_);
    }

    inline uint8_t *rawStubCode() const {
        return stubCode_;
    }

    // This method is not valid on TypeUpdate stub chains!
    inline ICFallbackStub *getChainFallback() {
        ICStub *lastStub = this;
        while (lastStub->next_)
            lastStub = lastStub->next_;
        JS_ASSERT(lastStub->isFallback());
        return lastStub->toFallbackStub();
    }

    static inline size_t offsetOfNext() {
        return offsetof(ICStub, next_);
    }

    static inline size_t offsetOfStubCode() {
        return offsetof(ICStub, stubCode_);
    }

    static inline size_t offsetOfExtra() {
        return offsetof(ICStub, extra_);
    }

    static bool CanMakeCalls(ICStub::Kind kind) {
        switch (kind) {
          case Call_Scripted:
          case Call_Native:
          case Call_Fallback:
          case UseCount_Fallback:
            return true;
          default:
            return false;
        }
    }

    // Optimized stubs get purged on GC.  But some stubs can be active on the
    // stack during GC - specifically the ones that can make calls.  To ensure
    // that these do not get purged, all stubs that can make calls are allocated
    // in the fallback stub space.
    bool allocatedInFallbackSpace() const {
        JS_ASSERT(next());
        return CanMakeCalls(kind());
    }
};

class ICFallbackStub : public ICStub
{
  protected:
    // Fallback stubs need these fields to easily add new stubs to
    // the linked list of stubs for an IC.

    // The IC entry for this linked list of stubs.
    ICEntry *icEntry_;

    // The number of stubs kept in the IC entry.
    uint32_t numOptimizedStubs_;

    // A pointer to the location stub pointer that needs to be
    // changed to add a new "last" stub immediately before the fallback
    // stub.  This'll start out pointing to the icEntry's "firstStub_"
    // field, and as new stubs are addd, it'll point to the current
    // last stub's "next_" field.
    ICStub **lastStubPtrAddr_;

    ICFallbackStub(Kind kind, IonCode *stubCode)
      : ICStub(kind, ICStub::Fallback, stubCode),
        icEntry_(NULL),
        numOptimizedStubs_(0),
        lastStubPtrAddr_(NULL) {}

    ICFallbackStub(Kind kind, Trait trait, IonCode *stubCode)
      : ICStub(kind, trait, stubCode),
        icEntry_(NULL),
        numOptimizedStubs_(0),
        lastStubPtrAddr_(NULL)
    {
        JS_ASSERT(trait == ICStub::Fallback ||
                  trait == ICStub::MonitoredFallback);
    }

  public:
    inline ICEntry *icEntry() const {
        return icEntry_;
    }

    inline size_t numOptimizedStubs() const {
        return (size_t) numOptimizedStubs_;
    }

    // The icEntry and lastStubPtrAddr_ fields can't be initialized when the stub is
    // created since the stub is created at compile time, and we won't know the IC entry
    // address until after compile when the BaselineScript is created.  This method
    // allows these fields to be fixed up at that point.
    void fixupICEntry(ICEntry *icEntry) {
        JS_ASSERT(icEntry_ == NULL);
        JS_ASSERT(lastStubPtrAddr_ == NULL);
        icEntry_ = icEntry;
        lastStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }

    // Add a new stub to the IC chain terminated by this fallback stub.
    void addNewStub(ICStub *stub) {
        JS_ASSERT(*lastStubPtrAddr_ == this);
        JS_ASSERT(stub->next() == NULL);
        stub->setNext(this);
        *lastStubPtrAddr_ = stub;
        lastStubPtrAddr_ = stub->addressOfNext();
        numOptimizedStubs_++;
    }
    bool hasStub(ICStub::Kind kind) {
        ICStub *stub = icEntry_->firstStub();
        do {
            if (stub->kind() == kind)
                return true;

            stub = stub->next();
        } while (stub);

        return false;
    }

    void unlinkStub(ICStub *prev, ICStub *stub);
    void unlinkStubsWithKind(ICStub::Kind kind);
};

// Monitored stubs are IC stubs that feed a single resulting value out to a
// type monitor operation.
class ICMonitoredStub : public ICStub
{
  protected:
    // Pointer to the start of the type monitoring stub chain.
    ICStub *firstMonitorStub_;

    ICMonitoredStub(Kind kind, IonCode *stubCode, ICStub *firstMonitorStub);

  public:
    inline void updateFirstMonitorStub(ICStub *monitorStub) {
        // This should only be called once: when the first optimized monitor stub
        // is added to the type monitor IC chain.
        JS_ASSERT(firstMonitorStub_ && firstMonitorStub_->isTypeMonitor_Fallback());
        firstMonitorStub_ = monitorStub;
    }
    inline void resetFirstMonitorStub(ICStub *monitorFallback) {
        JS_ASSERT(monitorFallback->isTypeMonitor_Fallback());
        firstMonitorStub_ = monitorFallback;
    }
    inline ICStub *firstMonitorStub() const {
        return firstMonitorStub_;
    }

    static inline size_t offsetOfFirstMonitorStub() {
        return offsetof(ICMonitoredStub, firstMonitorStub_);
    }
};

// Monitored fallback stubs - as the name implies.
class ICMonitoredFallbackStub : public ICFallbackStub
{
  protected:
    // Pointer to the fallback monitor stub.
    ICTypeMonitor_Fallback *fallbackMonitorStub_;

    ICMonitoredFallbackStub(Kind kind, IonCode *stubCode)
      : ICFallbackStub(kind, ICStub::MonitoredFallback, stubCode),
        fallbackMonitorStub_(NULL) {}

  public:
    bool initMonitoringChain(JSContext *cx, ICStubSpace *space);
    bool addMonitorStubForValue(JSContext *cx, HandleScript script, HandleValue val);

    inline ICTypeMonitor_Fallback *fallbackMonitorStub() const {
        return fallbackMonitorStub_;
    }

    static inline size_t offsetOfFallbackMonitorStub() {
        return offsetof(ICMonitoredFallbackStub, fallbackMonitorStub_);
    }
};

// Updated stubs are IC stubs that use a TypeUpdate IC to track
// the status of heap typesets that need to be updated.
class ICUpdatedStub : public ICStub
{
  protected:
    // Pointer to the start of the type updating stub chain.
    ICStub *firstUpdateStub_;

    static const uint32_t MAX_OPTIMIZED_STUBS = 8;
    uint32_t numOptimizedStubs_;

    ICUpdatedStub(Kind kind, IonCode *stubCode)
      : ICStub(kind, ICStub::Updated, stubCode),
        firstUpdateStub_(NULL),
        numOptimizedStubs_(0)
    {}

  public:
    bool initUpdatingChain(JSContext *cx, ICStubSpace *space);

    bool addUpdateStubForValue(JSContext *cx, HandleScript script, HandleObject obj, RawId id,
                               HandleValue val);

    void addOptimizedUpdateStub(ICStub *stub) {
        if (firstUpdateStub_->isTypeUpdate_Fallback()) {
            stub->setNext(firstUpdateStub_);
            firstUpdateStub_ = stub;
        } else {
            ICStub *iter = firstUpdateStub_;
            JS_ASSERT(iter->next() != NULL);
            while (!iter->next()->isTypeUpdate_Fallback())
                iter = iter->next();
            JS_ASSERT(iter->next()->next() == NULL);
            stub->setNext(iter->next());
            iter->setNext(stub);
        }

        numOptimizedStubs_++;
    }

    inline ICStub *firstUpdateStub() const {
        return firstUpdateStub_;
    }

    inline uint32_t numOptimizedStubs() const {
        return numOptimizedStubs_;
    }

    static inline size_t offsetOfFirstUpdateStub() {
        return offsetof(ICUpdatedStub, firstUpdateStub_);
    }
};

// Base class for stubcode compilers.
class ICStubCompiler
{
    // Prevent GC in the middle of stub compilation.
    js::gc::AutoSuppressGC suppressGC;

    mozilla::DebugOnly<bool> entersStubFrame_;

  protected:
    JSContext *cx;
    ICStub::Kind kind;

    // By default the stubcode key is just the kind.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind);
    }

    virtual bool generateStubCode(MacroAssembler &masm) = 0;
    virtual bool postGenerateStubCode(MacroAssembler &masm, Handle<IonCode *> genCode) {
        return true;
    }
    IonCode *getStubCode();

    ICStubCompiler(JSContext *cx, ICStub::Kind kind)
      : suppressGC(cx), entersStubFrame_(false), cx(cx), kind(kind)
    {}

    // Emits a tail call to a VMFunction wrapper.
    bool tailCallVM(const VMFunction &fun, MacroAssembler &masm);

    // Emits a normal (non-tail) call to a VMFunction wrapper.
    bool callVM(const VMFunction &fun, MacroAssembler &masm);

    // Emits a call to a type-update IC, assuming that the value to be
    // checked is already in R0.
    bool callTypeUpdateIC(MacroAssembler &masm, uint32_t objectOffset);

    // A stub frame is used when a stub wants to call into the VM without
    // performing a tail call. This is required for the return address
    // to pc mapping to work.
    void enterStubFrame(MacroAssembler &masm, Register scratch);
    void leaveStubFrame(MacroAssembler &masm, bool calledIntoIon = false);

    inline GeneralRegisterSet availableGeneralRegs(size_t numInputs) const {
        GeneralRegisterSet regs(GeneralRegisterSet::All());
        JS_ASSERT(!regs.has(BaselineStackReg));
#ifdef JS_CPU_ARM
        JS_ASSERT(!regs.has(BaselineTailCallReg));
#endif
        regs.take(BaselineFrameReg);
        regs.take(BaselineStubReg);
#ifdef JS_CPU_X64
        regs.take(ExtractTemp0);
        regs.take(ExtractTemp1);
#endif

        switch (numInputs) {
          case 0:
            break;
          case 1:
            regs.take(R0);
            break;
          case 2:
            regs.take(R0);
            regs.take(R1);
            break;
          default:
            JS_NOT_REACHED("Invalid numInputs");
        }

        return regs;
    }

  public:
    virtual ICStub *getStub(ICStubSpace *space) = 0;

    ICStubSpace *getStubSpace(HandleScript script) {
        return ICStub::CanMakeCalls(kind)
            ? script->baselineScript()->fallbackStubSpace()
            : script->baselineScript()->optimizedStubSpace();
    }
};

// Base class for stub compilers that can generate multiple stubcodes.
// These compilers need access to the JSOp they are compiling for.
class ICMultiStubCompiler : public ICStubCompiler
{
  protected:
    JSOp op;

    // Stub keys for multi-stub kinds are composed of both the kind
    // and the op they are compiled for.
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(op) << 16);
    }

    ICMultiStubCompiler(JSContext *cx, ICStub::Kind kind, JSOp op)
      : ICStubCompiler(cx, kind), op(op) {}
};

// StackCheck_Fallback

// A StackCheck IC chain has only the fallback stub.
class ICStackCheck_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICStackCheck_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::StackCheck_Fallback, stubCode)
    { }

  public:
    static inline ICStackCheck_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICStackCheck_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::StackCheck_Fallback)
        { }

        ICStackCheck_Fallback *getStub(ICStubSpace *space) {
            return ICStackCheck_Fallback::New(space, getStubCode());
        }
    };
};

// UseCount_Fallback

// A UseCount IC chain has only the fallback stub.
class ICUseCount_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICUseCount_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::UseCount_Fallback, stubCode)
    { }

  public:
    static inline ICUseCount_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICUseCount_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::UseCount_Fallback)
        { }

        ICUseCount_Fallback *getStub(ICStubSpace *space) {
            return ICUseCount_Fallback::New(space, getStubCode());
        }
    };
};

// TypeMonitor

// The TypeMonitor fallback stub is not always a regular fallback stub. When
// used for monitoring the values pushed by a bytecode it doesn't hold a
// pointer to the IC entry, but rather back to the main fallback stub for the
// IC (from which a pointer to the IC entry can be retrieved). When monitoring
// the types of 'this', arguments or other values with no associated IC, there
// is no main fallback stub, and the IC entry is referenced directly.
class ICTypeMonitor_Fallback : public ICStub
{
    friend class ICStubSpace;

    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    // Pointer to the main fallback stub for the IC or to the main IC entry,
    // depending on hasFallbackStub.
    union {
        ICMonitoredFallbackStub *mainFallbackStub_;
        ICEntry *icEntry_;
    };

    // Pointer to the first monitor stub.
    ICStub *firstMonitorStub_;

    // Address of the last monitor stub's field pointing to this
    // fallback monitor stub.  This will get updated when new
    // monitor stubs are created and added.
    ICStub **lastMonitorStubPtrAddr_;

    // Count of optimized type monitor stubs in this chain.
    uint32_t numOptimizedMonitorStubs_ : 8;

    // Whether this has a fallback stub referring to the IC entry.
    bool hasFallbackStub_ : 1;

    // Index of 'this' or argument which is being monitored, or BYTECODE_INDEX
    // if this is monitoring the types of values pushed at some bytecode.
    uint32_t argumentIndex_ : 23;

    static const uint32_t BYTECODE_INDEX = (1 << 23) - 1;

    ICTypeMonitor_Fallback(IonCode *stubCode, ICMonitoredFallbackStub *mainFallbackStub,
                           uint32_t argumentIndex)
      : ICStub(ICStub::TypeMonitor_Fallback, stubCode),
        mainFallbackStub_(mainFallbackStub),
        firstMonitorStub_(this),
        lastMonitorStubPtrAddr_(NULL),
        numOptimizedMonitorStubs_(0),
        hasFallbackStub_(mainFallbackStub != NULL),
        argumentIndex_(argumentIndex)
    { }

    void addOptimizedMonitorStub(ICStub *stub) {
        stub->setNext(this);

        JS_ASSERT((lastMonitorStubPtrAddr_ != NULL) ==
                  (numOptimizedMonitorStubs_ || !hasFallbackStub_));

        if (lastMonitorStubPtrAddr_)
            *lastMonitorStubPtrAddr_ = stub;

        if (numOptimizedMonitorStubs_ == 0) {
            JS_ASSERT(firstMonitorStub_ == this);
            firstMonitorStub_ = stub;
        } else {
            JS_ASSERT(firstMonitorStub_ != NULL);
        }

        lastMonitorStubPtrAddr_ = stub->addressOfNext();
        numOptimizedMonitorStubs_++;
    }

  public:
    static inline ICTypeMonitor_Fallback *New(
        ICStubSpace *space, IonCode *code, ICMonitoredFallbackStub *mainFbStub,
        uint32_t argumentIndex)
    {
        return space->allocate<ICTypeMonitor_Fallback>(code, mainFbStub, argumentIndex);
    }

    inline ICFallbackStub *mainFallbackStub() const {
        JS_ASSERT(hasFallbackStub_);
        return mainFallbackStub_;
    }

    inline ICEntry *icEntry() const {
        return hasFallbackStub_ ? mainFallbackStub()->icEntry() : icEntry_;
    }

    inline ICStub *firstMonitorStub() const {
        return firstMonitorStub_;
    }

    static inline size_t offsetOfFirstMonitorStub() {
        return offsetof(ICTypeMonitor_Fallback, firstMonitorStub_);
    }

    inline uint32_t numOptimizedMonitorStubs() const {
        return numOptimizedMonitorStubs_;
    }

    inline bool monitorsThis() const {
        return argumentIndex_ == 0;
    }

    inline bool monitorsArgument(uint32_t *pargument) const {
        if (argumentIndex_ > 0 && argumentIndex_ < BYTECODE_INDEX) {
            *pargument = argumentIndex_ - 1;
            return true;
        }
        return false;
    }

    inline bool monitorsBytecode() const {
        return argumentIndex_ == BYTECODE_INDEX;
    }

    // Fixup the IC entry as for a normal fallback stub, for this/arguments.
    void fixupICEntry(ICEntry *icEntry) {
        JS_ASSERT(!hasFallbackStub_);
        JS_ASSERT(icEntry_ == NULL);
        JS_ASSERT(lastMonitorStubPtrAddr_ == NULL);
        icEntry_ = icEntry;
        lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }

    // Create a new monitor stub for the type of the given value, and
    // add it to this chain.
    bool addMonitorStubForValue(JSContext *cx, HandleScript script, HandleValue val);

    void resetMonitorStubChain();

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
        ICMonitoredFallbackStub *mainFallbackStub_;
        uint32_t argumentIndex_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICMonitoredFallbackStub *mainFallbackStub)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(mainFallbackStub),
            argumentIndex_(BYTECODE_INDEX)
        { }

        Compiler(JSContext *cx, uint32_t argumentIndex)
          : ICStubCompiler(cx, ICStub::TypeMonitor_Fallback),
            mainFallbackStub_(NULL),
            argumentIndex_(argumentIndex)
        { }

        ICTypeMonitor_Fallback *getStub(ICStubSpace *space) {
            return ICTypeMonitor_Fallback::New(space, getStubCode(), mainFallbackStub_,
                                               argumentIndex_);
        }
    };
};

class ICTypeMonitor_Primitive : public ICStub
{
    friend class ICStubSpace;

    ICTypeMonitor_Primitive(IonCode *stubCode, JSValueType type)
        : ICStub(TypeMonitor_Primitive, stubCode)
    {
        extra_ = static_cast<uint16_t>(type);
    }

  public:
    static inline ICTypeMonitor_Primitive *New(ICStubSpace *space, IonCode *code,
                                               JSValueType type)
    {
        return space->allocate<ICTypeMonitor_Primitive>(code, type);
    }

    JSValueType type() const {
        return static_cast<JSValueType>(extra_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        JSValueType type_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16);
        }

      public:
        Compiler(JSContext *cx, JSValueType type)
          : ICStubCompiler(cx, TypeMonitor_Primitive),
            type_(type)
        { }

        ICTypeMonitor_Primitive *getStub(ICStubSpace *space) {
            return ICTypeMonitor_Primitive::New(space, getStubCode(), type_);
        }
    };
};

class ICTypeMonitor_SingleObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrObject obj_;

    ICTypeMonitor_SingleObject(IonCode *stubCode, HandleObject obj)
      : ICStub(TypeMonitor_SingleObject, stubCode),
        obj_(obj)
    { }

  public:
    static inline ICTypeMonitor_SingleObject *New(
            ICStubSpace *space, IonCode *code, HandleObject obj)
    {
        return space->allocate<ICTypeMonitor_SingleObject>(code, obj);
    }

    HeapPtrObject &object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeMonitor_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj)
          : ICStubCompiler(cx, TypeMonitor_SingleObject),
            obj_(obj)
        { }

        ICTypeMonitor_SingleObject *getStub(ICStubSpace *space) {
            return ICTypeMonitor_SingleObject::New(space, getStubCode(), obj_);
        }
    };
};

class ICTypeMonitor_TypeObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrTypeObject type_;

    ICTypeMonitor_TypeObject(IonCode *stubCode, HandleTypeObject type)
      : ICStub(TypeMonitor_TypeObject, stubCode),
        type_(type)
    { }

  public:
    static inline ICTypeMonitor_TypeObject *New(
            ICStubSpace *space, IonCode *code, HandleTypeObject type)
    {
        return space->allocate<ICTypeMonitor_TypeObject>(code, type);
    }

    HeapPtrTypeObject &type() {
        return type_;
    }

    static size_t offsetOfType() {
        return offsetof(ICTypeMonitor_TypeObject, type_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleTypeObject type_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleTypeObject type)
          : ICStubCompiler(cx, TypeMonitor_TypeObject),
            type_(type)
        { }

        ICTypeMonitor_TypeObject *getStub(ICStubSpace *space) {
            return ICTypeMonitor_TypeObject::New(space, getStubCode(), type_);
        }
    };
};

// TypeUpdate

extern const VMFunction DoTypeUpdateFallbackInfo;

// The TypeUpdate fallback is not a regular fallback, since it just
// forwards to a different entry point in the main fallback stub.
class ICTypeUpdate_Fallback : public ICStub
{
    friend class ICStubSpace;

    ICTypeUpdate_Fallback(IonCode *stubCode)
      : ICStub(ICStub::TypeUpdate_Fallback, stubCode)
    {}

  public:
    static inline ICTypeUpdate_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICTypeUpdate_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::TypeUpdate_Fallback)
        { }

        ICTypeUpdate_Fallback *getStub(ICStubSpace *space) {
            return ICTypeUpdate_Fallback::New(space, getStubCode());
        }
    };
};

// Type update stub to handle a primitive type.
class ICTypeUpdate_Primitive : public ICStub
{
    friend class ICStubSpace;

    ICTypeUpdate_Primitive(IonCode *stubCode, JSValueType type)
        : ICStub(TypeUpdate_Primitive, stubCode)
    {
        extra_ = static_cast<uint16_t>(type);
    }

  public:
    static inline ICTypeUpdate_Primitive *New(ICStubSpace *space,  IonCode *code,
                                              JSValueType type)
    {
        return space->allocate<ICTypeUpdate_Primitive>(code, type);
    }

    JSValueType type() const {
        return static_cast<JSValueType>(extra_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        JSValueType type_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16);
        }

      public:
        Compiler(JSContext *cx, JSValueType type)
          : ICStubCompiler(cx, TypeUpdate_Primitive),
            type_(type)
        { }

        ICTypeUpdate_Primitive *getStub(ICStubSpace *space) {
            return ICTypeUpdate_Primitive::New(space, getStubCode(), type_);
        }
    };
};

// Type update stub to handle a singleton object.
class ICTypeUpdate_SingleObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrObject obj_;

    ICTypeUpdate_SingleObject(IonCode *stubCode, HandleObject obj)
      : ICStub(TypeUpdate_SingleObject, stubCode),
        obj_(obj)
    { }

  public:
    static inline ICTypeUpdate_SingleObject *New(ICStubSpace *space, IonCode *code,
                                                 HandleObject obj)
    {
        return space->allocate<ICTypeUpdate_SingleObject>(code, obj);
    }

    HeapPtrObject &object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeUpdate_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleObject obj)
          : ICStubCompiler(cx, TypeUpdate_SingleObject),
            obj_(obj)
        { }

        ICTypeUpdate_SingleObject *getStub(ICStubSpace *space) {
            return ICTypeUpdate_SingleObject::New(space, getStubCode(), obj_);
        }
    };
};

// Type update stub to handle a single TypeObject.
class ICTypeUpdate_TypeObject : public ICStub
{
    friend class ICStubSpace;

    HeapPtrTypeObject type_;

    ICTypeUpdate_TypeObject(IonCode *stubCode, HandleTypeObject type)
      : ICStub(TypeUpdate_TypeObject, stubCode),
        type_(type)
    { }

  public:
    static inline ICTypeUpdate_TypeObject *New(ICStubSpace *space, IonCode *code,
                                               HandleTypeObject type)
    {
        return space->allocate<ICTypeUpdate_TypeObject>(code, type);
    }

    HeapPtrTypeObject &type() {
        return type_;
    }

    static size_t offsetOfType() {
        return offsetof(ICTypeUpdate_TypeObject, type_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleTypeObject type_;
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleTypeObject type)
          : ICStubCompiler(cx, TypeUpdate_TypeObject),
            type_(type)
        { }

        ICTypeUpdate_TypeObject *getStub(ICStubSpace *space) {
            return ICTypeUpdate_TypeObject::New(space, getStubCode(), type_);
        }
    };
};

// This
//      JSOP_THIS

class ICThis_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICThis_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::This_Fallback, stubCode) {}

  public:
    static inline ICThis_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICThis_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::This_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICThis_Fallback::New(space, getStubCode());
        }
    };
};

class ICNewArray_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICNewArray_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::NewArray_Fallback, stubCode)
    {}

  public:
    static inline ICNewArray_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICNewArray_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::NewArray_Fallback)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICNewArray_Fallback::New(space, getStubCode());
        }
    };
};

class ICNewObject_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICNewObject_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::NewObject_Fallback, stubCode)
    {}

  public:
    static inline ICNewObject_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICNewObject_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::NewObject_Fallback)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICNewObject_Fallback::New(space, getStubCode());
        }
    };
};

// Compare
//      JSOP_LT
//      JSOP_GT

class ICCompare_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICCompare_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::Compare_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICCompare_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::Compare_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Fallback::New(space, getStubCode());
        }
    };
};

class ICCompare_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Int32(IonCode *stubCode)
      : ICStub(ICStub::Compare_Int32, stubCode) {}

  public:
    static inline ICCompare_Int32 *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_Int32>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Int32, op) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Int32::New(space, getStubCode());
        }
    };
};

class ICCompare_Double : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Double(IonCode *stubCode)
      : ICStub(ICStub::Compare_Double, stubCode)
    {}

  public:
    static inline ICCompare_Double *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_Double>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Double, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Double::New(space, getStubCode());
        }
    };
};

class ICCompare_NumberWithUndefined : public ICStub
{
    friend class ICStubSpace;

    ICCompare_NumberWithUndefined(IonCode *stubCode)
      : ICStub(ICStub::Compare_NumberWithUndefined, stubCode)
    {}

  public:
    static inline ICCompare_NumberWithUndefined *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_NumberWithUndefined>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

        bool lhsIsUndefined;

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsUndefined)
          : ICMultiStubCompiler(cx, ICStub::Compare_NumberWithUndefined, op),
            lhsIsUndefined(lhsIsUndefined)
        {}

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind)
                 | (static_cast<int32_t>(op) << 16)
                 | (static_cast<int32_t>(lhsIsUndefined) << 24);
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_NumberWithUndefined::New(space, getStubCode());
        }
    };
};

class ICCompare_String : public ICStub
{
    friend class ICStubSpace;

    ICCompare_String(IonCode *stubCode)
      : ICStub(ICStub::Compare_String, stubCode)
    {}

  public:
    static inline ICCompare_String *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_String>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_String, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_String::New(space, getStubCode());
        }
    };
};

class ICCompare_Boolean : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Boolean(IonCode *stubCode)
      : ICStub(ICStub::Compare_Boolean, stubCode)
    {}

  public:
    static inline ICCompare_Boolean *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_Boolean>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Boolean, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Boolean::New(space, getStubCode());
        }
    };
};

class ICCompare_Object : public ICStub
{
    friend class ICStubSpace;

    ICCompare_Object(IonCode *stubCode)
      : ICStub(ICStub::Compare_Object, stubCode)
    {}

  public:
    static inline ICCompare_Object *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_Object>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::Compare_Object, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_Object::New(space, getStubCode());
        }
    };
};

class ICCompare_ObjectWithUndefined : public ICStub
{
    friend class ICStubSpace;

    ICCompare_ObjectWithUndefined(IonCode *stubCode)
      : ICStub(ICStub::Compare_ObjectWithUndefined, stubCode)
    {}

  public:
    static inline ICCompare_ObjectWithUndefined *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICCompare_ObjectWithUndefined>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

        bool lhsIsUndefined;
        bool compareWithNull;

      public:
        Compiler(JSContext *cx, JSOp op, bool lhsIsUndefined, bool compareWithNull)
          : ICMultiStubCompiler(cx, ICStub::Compare_ObjectWithUndefined, op),
            lhsIsUndefined(lhsIsUndefined),
            compareWithNull(compareWithNull)
        {}

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind)
                 | (static_cast<int32_t>(op) << 16)
                 | (static_cast<int32_t>(lhsIsUndefined) << 24)
                 | (static_cast<int32_t>(compareWithNull) << 25);
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICCompare_NumberWithUndefined::New(space, getStubCode());
        }
    };
};

// ToBool
//      JSOP_IFNE

class ICToBool_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICToBool_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToBool_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICToBool_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICToBool_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_Fallback::New(space, getStubCode());
        }
    };
};

class ICToBool_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICToBool_Int32(IonCode *stubCode)
      : ICStub(ICStub::ToBool_Int32, stubCode) {}

  public:
    static inline ICToBool_Int32 *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICToBool_Int32>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_Int32) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_Int32::New(space, getStubCode());
        }
    };
};

class ICToBool_String : public ICStub
{
    friend class ICStubSpace;

    ICToBool_String(IonCode *stubCode)
      : ICStub(ICStub::ToBool_String, stubCode) {}

  public:
    static inline ICToBool_String *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICToBool_String>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_String) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_String::New(space, getStubCode());
        }
    };
};

class ICToBool_NullUndefined : public ICStub
{
    friend class ICStubSpace;

    ICToBool_NullUndefined(IonCode *stubCode)
      : ICStub(ICStub::ToBool_NullUndefined, stubCode) {}

  public:
    static inline ICToBool_NullUndefined *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICToBool_NullUndefined>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToBool_NullUndefined) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToBool_NullUndefined::New(space, getStubCode());
        }
    };
};

// ToNumber
//     JSOP_POS

class ICToNumber_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICToNumber_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::ToNumber_Fallback, stubCode) {}

  public:
    static inline ICToNumber_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICToNumber_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::ToNumber_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICToNumber_Fallback::New(space, getStubCode());
        }
    };
};

// BinaryArith
//      JSOP_ADD
//      JSOP_BITAND, JSOP_BITXOR, JSOP_BITOR
//      JSOP_LSH, JSOP_RSH, JSOP_URSH

class ICBinaryArith_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICBinaryArith_Fallback(IonCode *stubCode)
      : ICFallbackStub(BinaryArith_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICBinaryArith_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBinaryArith_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BinaryArith_Fallback) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_Fallback::New(space, getStubCode());
        }
    };
};

class ICBinaryArith_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_Int32(IonCode *stubCode)
      : ICStub(BinaryArith_Int32, stubCode) {}

  public:
    static inline ICBinaryArith_Int32 *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBinaryArith_Int32>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        JSOp op_;
        bool allowDouble_;

        bool generateStubCode(MacroAssembler &masm);

        // Stub keys shift-stubs need to encode the kind, the JSOp and if we allow doubles.
        virtual int32_t getKey() const {
            return (static_cast<int32_t>(kind) | (static_cast<int32_t>(op_) << 16) |
                    (static_cast<int32_t>(allowDouble_) << 24));
        }

      public:
        Compiler(JSContext *cx, JSOp op, bool allowDouble)
          : ICStubCompiler(cx, ICStub::BinaryArith_Int32),
            op_(op), allowDouble_(allowDouble) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_Int32::New(space, getStubCode());
        }
    };
};

class ICBinaryArith_StringConcat : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_StringConcat(IonCode *stubCode)
      : ICStub(BinaryArith_StringConcat, stubCode)
    {}

  public:
    static inline ICBinaryArith_StringConcat *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBinaryArith_StringConcat>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BinaryArith_StringConcat)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_StringConcat::New(space, getStubCode());
        }
    };
};

class ICBinaryArith_StringObjectConcat : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_StringObjectConcat(IonCode *stubCode, bool lhsIsString)
      : ICStub(BinaryArith_StringObjectConcat, stubCode)
    {
        extra_ = lhsIsString;
    }

  public:
    static inline ICBinaryArith_StringObjectConcat *New(ICStubSpace *space, IonCode *code,
                                                        bool lhsIsString) {
        return space->allocate<ICBinaryArith_StringObjectConcat>(code, lhsIsString);
    }

    bool lhsIsString() const {
        return extra_;
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool lhsIsString_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(lhsIsString_) << 16);
        }

      public:
        Compiler(JSContext *cx, bool lhsIsString)
          : ICStubCompiler(cx, ICStub::BinaryArith_StringObjectConcat),
            lhsIsString_(lhsIsString)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_StringObjectConcat::New(space, getStubCode(), lhsIsString_);
        }
    };
};

class ICBinaryArith_Double : public ICStub
{
    friend class ICStubSpace;

    ICBinaryArith_Double(IonCode *stubCode)
      : ICStub(BinaryArith_Double, stubCode)
    {}

  public:
    static inline ICBinaryArith_Double *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBinaryArith_Double>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::BinaryArith_Double, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICBinaryArith_Double::New(space, getStubCode());
        }
    };
};

// UnaryArith
//     JSOP_BITNOT
//     JSOP_NEG

class ICUnaryArith_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICUnaryArith_Fallback(IonCode *stubCode)
      : ICFallbackStub(UnaryArith_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICUnaryArith_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICUnaryArith_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::UnaryArith_Fallback)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICUnaryArith_Fallback::New(space, getStubCode());
        }
    };
};

class ICUnaryArith_Int32 : public ICStub
{
    friend class ICStubSpace;

    ICUnaryArith_Int32(IonCode *stubCode)
      : ICStub(UnaryArith_Int32, stubCode)
    {}

  public:
    static inline ICUnaryArith_Int32 *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICUnaryArith_Int32>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::UnaryArith_Int32, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICUnaryArith_Int32::New(space, getStubCode());
        }
    };
};

class ICUnaryArith_Double : public ICStub
{
    friend class ICStubSpace;

    ICUnaryArith_Double(IonCode *stubCode)
      : ICStub(UnaryArith_Int32, stubCode)
    {}

  public:
    static inline ICUnaryArith_Double *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICUnaryArith_Double>(code);
    }

    class Compiler : public ICMultiStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, JSOp op)
          : ICMultiStubCompiler(cx, ICStub::UnaryArith_Int32, op)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICUnaryArith_Double::New(space, getStubCode());
        }
    };
};

// GetElem
//      JSOP_GETELEM

class ICGetElem_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetElem_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 16;

    static inline ICGetElem_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetElem_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetElem_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetElem_Fallback *stub = ICGetElem_Fallback::New(space, getStubCode());
            if (!stub)
                return NULL;
            if (!stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICGetElemNativeStub : public ICMonitoredStub
{
    HeapPtrShape shape_;
    HeapValue idval_;
    uint32_t offset_;

  protected:
    ICGetElemNativeStub(ICStub::Kind kind, IonCode *stubCode, ICStub *firstMonitorStub,
                        HandleShape shape, HandleValue idval,
                        bool isFixedSlot, uint32_t offset)
      : ICMonitoredStub(kind, stubCode, firstMonitorStub),
        shape_(shape),
        idval_(idval),
        offset_(offset)
    {
        extra_ = isFixedSlot;
    }

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICGetElemNativeStub, shape_);
    }

    HeapValue &idval() {
        return idval_;
    }
    static size_t offsetOfIdval() {
        return offsetof(ICGetElemNativeStub, idval_);
    }

    uint32_t offset() const {
        return offset_;
    }
    static size_t offsetOfOffset() {
        return offsetof(ICGetElemNativeStub, offset_);
    }

    bool isFixedSlot() const {
        return extra_;
    }
};

class ICGetElem_Native : public ICGetElemNativeStub
{
    friend class ICStubSpace;
    ICGetElem_Native(IonCode *stubCode, ICStub *firstMonitorStub,
                     HandleShape shape, HandleValue idval,
                     bool isFixedSlot, uint32_t offset)
      : ICGetElemNativeStub(ICStub::GetElem_Native, stubCode, firstMonitorStub, shape, idval,
                            isFixedSlot, offset)
    {}

  public:
    static inline ICGetElem_Native *New(ICStubSpace *space, IonCode *code,
                                        ICStub *firstMonitorStub,
                                        HandleShape shape, HandleValue idval,
                                        bool isFixedSlot, uint32_t offset)
    {
        return space->allocate<ICGetElem_Native>(code, firstMonitorStub, shape, idval,
                                                 isFixedSlot, offset);
    }
};

class ICGetElem_NativePrototype : public ICGetElemNativeStub
{
    friend class ICStubSpace;
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    ICGetElem_NativePrototype(IonCode *stubCode, ICStub *firstMonitorStub,
                              HandleShape shape, HandleValue idval,
                              bool isFixedSlot, uint32_t offset,
                              HandleObject holder, HandleShape holderShape)
      : ICGetElemNativeStub(ICStub::GetElem_NativePrototype, stubCode, firstMonitorStub, shape,
                            idval, isFixedSlot, offset),
        holder_(holder),
        holderShape_(holderShape)
    {}

  public:
    static inline ICGetElem_NativePrototype *New(ICStubSpace *space, IonCode *code,
                                                 ICStub *firstMonitorStub,
                                                 HandleShape shape, HandleValue idval,
                                                 bool isFixedSlot, uint32_t offset,
                                                 HandleObject holder, HandleShape holderShape)
    {
        return space->allocate<ICGetElem_NativePrototype>(code, firstMonitorStub, shape, idval,
                                                          isFixedSlot, offset, holder, holderShape);
    }

    HeapPtrObject &holder() {
        return holder_;
    }
    static size_t offsetOfHolder() {
        return offsetof(ICGetElem_NativePrototype, holder_);
    }

    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICGetElem_NativePrototype, holderShape_);
    }
};

// Compiler for GetElem_Native and GetElem_NativePrototype stubs.
class ICGetElemNativeCompiler : public ICStubCompiler
{
    ICStub *firstMonitorStub_;
    HandleObject obj_;
    HandleObject holder_;
    HandleValue idval_;
    bool isFixedSlot_;
    uint32_t offset_;

    bool generateStubCode(MacroAssembler &masm);

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
    }

  public:
    ICGetElemNativeCompiler(JSContext *cx, ICStub::Kind kind, ICStub *firstMonitorStub,
                            HandleObject obj, HandleObject holder, HandleValue idval,
                            bool isFixedSlot, uint32_t offset)
      : ICStubCompiler(cx, kind),
        firstMonitorStub_(firstMonitorStub),
        obj_(obj),
        holder_(holder),
        idval_(idval),
        isFixedSlot_(isFixedSlot),
        offset_(offset)
    {}

    ICStub *getStub(ICStubSpace *space) {
        RootedShape shape(cx, obj_->lastProperty());
        if (kind == ICStub::GetElem_Native) {
            JS_ASSERT(obj_ == holder_);
            return ICGetElem_Native::New(space, getStubCode(), firstMonitorStub_, shape, idval_,
                                         isFixedSlot_, offset_);
        }

        JS_ASSERT(obj_ != holder_);
        JS_ASSERT(kind == ICStub::GetElem_NativePrototype);
        RootedShape holderShape(cx, holder_->lastProperty());
        return ICGetElem_NativePrototype::New(space, getStubCode(), firstMonitorStub_, shape,
                                              idval_, isFixedSlot_, offset_, holder_, holderShape);
    }
};

class ICGetElem_String : public ICStub
{
    friend class ICStubSpace;

    ICGetElem_String(IonCode *stubCode)
      : ICStub(ICStub::GetElem_String, stubCode) {}

  public:
    static inline ICGetElem_String *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetElem_String>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetElem_String) {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_String::New(space, getStubCode());
        }
    };
};

class ICGetElem_Dense : public ICMonitoredStub
{
    friend class ICStubSpace;

    HeapPtrShape shape_;

    ICGetElem_Dense(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape)
      : ICMonitoredStub(GetElem_Dense, stubCode, firstMonitorStub),
        shape_(shape)
    {}

  public:
    static inline ICGetElem_Dense *New(ICStubSpace *space, IonCode *code,
                                       ICStub *firstMonitorStub, HandleShape shape)
    {
        return space->allocate<ICGetElem_Dense>(code, firstMonitorStub, shape);
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetElem_Dense, shape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
      ICStub *firstMonitorStub_;
      RootedShape shape_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, UnrootedShape shape)
          : ICStubCompiler(cx, ICStub::GetElem_Dense),
            firstMonitorStub_(firstMonitorStub),
            shape_(cx, shape)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_Dense::New(space, getStubCode(), firstMonitorStub_, shape_);
        }
    };
};

class ICGetElem_TypedArray : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrShape shape_;

    ICGetElem_TypedArray(IonCode *stubCode, HandleShape shape, uint32_t type)
      : ICStub(GetElem_TypedArray, stubCode),
        shape_(shape)
    {
        extra_ = uint16_t(type);
        JS_ASSERT(extra_ == type);
    }

  public:
    static inline ICGetElem_TypedArray *New(ICStubSpace *space, IonCode *code,
                                            HandleShape shape, uint32_t type)
    {
        return space->allocate<ICGetElem_TypedArray>(code, shape, type);
    }

    static size_t offsetOfShape() {
        return offsetof(ICGetElem_TypedArray, shape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
      RootedShape shape_;
      uint32_t type_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16);
        }

      public:
        Compiler(JSContext *cx, UnrootedShape shape, uint32_t type)
          : ICStubCompiler(cx, ICStub::GetElem_TypedArray),
            shape_(cx, shape),
            type_(type)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetElem_TypedArray::New(space, getStubCode(), shape_, type_);
        }
    };
};

// SetElem
//      JSOP_SETELEM
//      JSOP_INITELEM

class ICSetElem_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICSetElem_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetElem_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICSetElem_Fallback>(code);
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetElem_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICSetElem_Fallback::New(space, getStubCode());
        }
    };
};

class ICSetElem_Dense : public ICUpdatedStub
{
    friend class ICStubSpace;

    HeapPtrShape shape_;
    HeapPtrTypeObject type_;

    ICSetElem_Dense(IonCode *stubCode, HandleShape shape, HandleTypeObject type)
      : ICUpdatedStub(SetElem_Dense, stubCode),
        shape_(shape),
        type_(type)
    {}

  public:
    static inline ICSetElem_Dense *New(ICStubSpace *space, IonCode *code, HandleShape shape,
                                       HandleTypeObject type) {
        return space->allocate<ICSetElem_Dense>(code, shape, type);
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetElem_Dense, shape_);
    }
    static size_t offsetOfType() {
        return offsetof(ICSetElem_Dense, type_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrTypeObject &type() {
        return type_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;

        // Compiler is only live on stack during compilation, it should
        // outlive any RootedTypeObject it's passed.  So it can just
        // use the handle.
        HandleTypeObject type_;

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, UnrootedShape shape, HandleTypeObject type)
          : ICStubCompiler(cx, ICStub::SetElem_Dense),
            shape_(cx, shape),
            type_(type)
        {}

        ICUpdatedStub *getStub(ICStubSpace *space) {
            ICSetElem_Dense *stub = ICSetElem_Dense::New(space, getStubCode(), shape_, type_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICSetElem_DenseAdd : public ICUpdatedStub
{
    friend class ICStubSpace;

    HeapPtrShape shape_;
    HeapPtrTypeObject type_;
    HeapPtrObject lastProto_;
    HeapPtrShape lastProtoShape_;

    ICSetElem_DenseAdd(IonCode *stubCode, HandleShape shape, HandleTypeObject type,
                       HandleObject lastProto, HandleShape lastProtoShape)
      : ICUpdatedStub(SetElem_DenseAdd, stubCode),
        shape_(shape),
        type_(type),
        lastProto_(lastProto),
        lastProtoShape_(lastProtoShape)
    {}

  public:
    static inline ICSetElem_DenseAdd *New(ICStubSpace *space, IonCode *code,
                                          HandleShape shape, HandleTypeObject type,
                                          HandleObject lastProto, HandleShape lastProtoShape)
    {
        return space->allocate<ICSetElem_DenseAdd>(code, shape, type, lastProto, lastProtoShape);
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetElem_DenseAdd, shape_);
    }
    static size_t offsetOfType() {
        return offsetof(ICSetElem_DenseAdd, type_);
    }
    static size_t offsetOfLastProto() {
        return offsetof(ICSetElem_DenseAdd, lastProto_);
    }
    static size_t offsetOfLastProtoShape() {
        return offsetof(ICSetElem_DenseAdd, lastProtoShape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }
    HeapPtrTypeObject &type() {
        return type_;
    }
    HeapPtrObject &lastProto() {
        return lastProto_;
    }
    HeapPtrShape &lastProtoShape() {
        return lastProtoShape_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;

        // Compiler is only live on stack during compilation, it should
        // outlive any RootedTypeObject it's passed.  So it can just
        // use the handle.
        HandleTypeObject type_;

        RootedObject lastProto_;
        RootedShape lastProtoShape_;

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, UnrootedShape shape, HandleTypeObject type,
                 UnrootedObject lastProto, UnrootedShape lastProtoShape)
          : ICStubCompiler(cx, ICStub::SetElem_DenseAdd),
            shape_(cx, shape),
            type_(type),
            lastProto_(cx, lastProto),
            lastProtoShape_(cx, lastProtoShape)
        {}

        ICUpdatedStub *getStub(ICStubSpace *space) {
            ICSetElem_DenseAdd *stub = ICSetElem_DenseAdd::New(space, getStubCode(), shape_, type_,
                                                               lastProto_, lastProtoShape_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICSetElem_TypedArray : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrShape shape_;

    ICSetElem_TypedArray(IonCode *stubCode, HandleShape shape, uint32_t type)
      : ICStub(SetElem_TypedArray, stubCode),
        shape_(shape)
    {
        extra_ = uint16_t(type);
        JS_ASSERT(extra_ == type);
    }

  public:
    static inline ICSetElem_TypedArray *New(ICStubSpace *space, IonCode *code,
                                            HandleShape shape, uint32_t type)
    {
        return space->allocate<ICSetElem_TypedArray>(code, shape, type);
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetElem_TypedArray, shape_);
    }

    HeapPtrShape &shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;
        uint32_t type_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(type_) << 16);
        }

      public:
        Compiler(JSContext *cx, UnrootedShape shape, uint32_t type)
          : ICStubCompiler(cx, ICStub::SetElem_TypedArray),
            shape_(cx, shape),
            type_(type)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICSetElem_TypedArray::New(space, getStubCode(), shape_, type_);
        }
    };
};

// In
//      JSOP_IN
class ICIn_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIn_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::In_Fallback, stubCode)
    { }

  public:
    static inline ICIn_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIn_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::In_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIn_Fallback::New(space, getStubCode());
        }
    };
};

// GetName
//      JSOP_NAME
//      JSOP_CALLNAME
//      JSOP_GETGNAME
//      JSOP_CALLGNAME
class ICGetName_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetName_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetName_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetName_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetName_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetName_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetName_Fallback *stub = ICGetName_Fallback::New(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// Optimized GETGNAME/CALLGNAME stub.
class ICGetName_Global : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrShape shape_;
    uint32_t slot_;

    ICGetName_Global(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape, uint32_t slot)
      : ICMonitoredStub(GetName_Global, stubCode, firstMonitorStub),
        shape_(shape),
        slot_(slot)
    {}

  public:
    static inline ICGetName_Global *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                        HandleShape shape, uint32_t slot)
    {
        return space->allocate<ICGetName_Global>(code, firstMonitorStub, shape, slot);
    }

    HeapPtrShape &shape() {
        return shape_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICGetName_Global, shape_);
    }
    static size_t offsetOfSlot() {
        return offsetof(ICGetName_Global, slot_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        RootedShape shape_;
        uint32_t slot_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, Shape *shape, uint32_t slot)
          : ICStubCompiler(cx, ICStub::GetName_Global),
            firstMonitorStub_(firstMonitorStub),
            shape_(cx, shape),
            slot_(slot)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetName_Global::New(space, getStubCode(), firstMonitorStub_, shape_, slot_);
        }
    };
};

// Optimized GETNAME/CALLNAME stub, making a variable number of hops to get an
// 'own' property off some scope object. Unlike GETPROP on an object's
// prototype, there is no teleporting optimization to take advantage of and
// shape checks are required all along the scope chain.
template <size_t NumHops>
class ICGetName_Scope : public ICMonitoredStub
{
    friend class ICStubSpace;

    static const size_t MAX_HOPS = 4;

    HeapPtrShape shapes_[NumHops + 1];
    uint32_t offset_;

    ICGetName_Scope(IonCode *stubCode, ICStub *firstMonitorStub,
                    AutoShapeVector *shapes, uint32_t offset)
      : ICMonitoredStub(GetStubKind(), stubCode, firstMonitorStub),
        offset_(offset)
    {
        JS_STATIC_ASSERT(NumHops <= MAX_HOPS);
        JS_ASSERT(shapes->length() == NumHops + 1);
        for (size_t i = 0; i < NumHops + 1; i++)
            shapes_[i].init((*shapes)[i]);
    }

    static Kind GetStubKind() {
        return (Kind) (GetName_Scope0 + NumHops);
    }

  public:
    static inline ICGetName_Scope *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                       AutoShapeVector *shapes, uint32_t offset)
    {
        return space->allocate<ICGetName_Scope<NumHops> >(code, firstMonitorStub, shapes, offset);
    }

    void traceScopes(JSTracer *trc) {
        for (size_t i = 0; i <= NumHops; i++)
            MarkShape(trc, &shapes_[i], "baseline-scope-stub-shape");
    }

    static size_t offsetOfShape(size_t index) {
        JS_ASSERT(index <= NumHops);
        return offsetof(ICGetName_Scope, shapes_) + (index * sizeof(HeapPtrShape));
    }
    static size_t offsetOfOffset() {
        return offsetof(ICGetName_Scope, offset_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        AutoShapeVector *shapes_;
        bool isFixedSlot_;
        uint32_t offset_;

      protected:
        bool generateStubCode(MacroAssembler &masm);

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub,
                 AutoShapeVector *shapes, bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, GetStubKind()),
            firstMonitorStub_(firstMonitorStub),
            shapes_(shapes),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {
        }

        ICStub *getStub(ICStubSpace *space) {
            return ICGetName_Scope::New(space, getStubCode(), firstMonitorStub_, shapes_, offset_);
        }
    };
};

// BindName
//      JSOP_BINDNAME
class ICBindName_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICBindName_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::BindName_Fallback, stubCode)
    { }

  public:
    static inline ICBindName_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICBindName_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::BindName_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICBindName_Fallback::New(space, getStubCode());
        }
    };
};

// GetIntrinsic
//      JSOP_GETINTRINSIC
//      JSOP_CALLINTRINSIC
class ICGetIntrinsic_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICGetIntrinsic_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::GetIntrinsic_Fallback, stubCode)
    { }

  public:
    static inline ICGetIntrinsic_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetIntrinsic_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICGetIntrinsic_Fallback::New(space, getStubCode());
        }
    };
};

// Stub that loads the constant result of a GETINTRINSIC operation.
class ICGetIntrinsic_Constant : public ICStub
{
    friend class ICStubSpace;

    HeapValue value_;

    ICGetIntrinsic_Constant(IonCode *stubCode, HandleValue value)
      : ICStub(GetIntrinsic_Constant, stubCode),
        value_(value)
    {}

  public:
    static inline ICGetIntrinsic_Constant *New(ICStubSpace *space, IonCode *code, HandleValue value) {
        return space->allocate<ICGetIntrinsic_Constant>(code, value);
    }

    HeapValue &value() {
        return value_;
    }
    static size_t offsetOfValue() {
        return offsetof(ICGetIntrinsic_Constant, value_);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

        HandleValue value_;

      public:
        Compiler(JSContext *cx, HandleValue value)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Constant),
            value_(value)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetIntrinsic_Constant::New(space, getStubCode(), value_);
        }
    };
};

class ICGetProp_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    ICGetProp_Fallback(IonCode *stubCode)
      : ICMonitoredFallbackStub(ICStub::GetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICGetProp_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetProp_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICGetProp_Fallback *stub = ICGetProp_Fallback::New(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// Stub for accessing a dense array's length.
class ICGetProp_ArrayLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_ArrayLength(IonCode *stubCode)
      : ICStub(GetProp_ArrayLength, stubCode)
    {}

  public:
    static inline ICGetProp_ArrayLength *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetProp_ArrayLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_ArrayLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_ArrayLength::New(space, getStubCode());
        }
    };
};

// Stub for accessing a typed array's length.
class ICGetProp_TypedArrayLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_TypedArrayLength(IonCode *stubCode)
      : ICStub(GetProp_TypedArrayLength, stubCode)
    {}

  public:
    static inline ICGetProp_TypedArrayLength *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetProp_TypedArrayLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_TypedArrayLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_TypedArrayLength::New(space, getStubCode());
        }
    };
};

// Stub for accessing a string's length.
class ICGetProp_String : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    // Shape of String.prototype to check for.
    HeapPtrShape stringProtoShape_;

    // Fixed or dynamic slot offset.
    uint32_t offset_;

    ICGetProp_String(IonCode *stubCode, ICStub *firstMonitorStub,
                     HandleShape stringProtoShape, uint32_t offset)
      : ICMonitoredStub(GetProp_String, stubCode, firstMonitorStub),
        stringProtoShape_(stringProtoShape),
        offset_(offset)
    {}

  public:
    static inline ICGetProp_String *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                        HandleShape stringProtoShape, uint32_t offset)
    {
        return space->allocate<ICGetProp_String>(code, firstMonitorStub, stringProtoShape, offset);
    }

    HeapPtrShape &stringProtoShape() {
        return stringProtoShape_;
    }
    static size_t offsetOfStringProtoShape() {
        return offsetof(ICGetProp_String, stringProtoShape_);
    }

    static size_t offsetOfOffset() {
        return offsetof(ICGetProp_String, offset_);
    }

    class Compiler : public ICStubCompiler {
        ICStub *firstMonitorStub_;
        RootedObject stringPrototype_;
        bool isFixedSlot_;
        uint32_t offset_;

        bool generateStubCode(MacroAssembler &masm);

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleObject stringPrototype,
                 bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, ICStub::GetProp_String),
            firstMonitorStub_(firstMonitorStub),
            stringPrototype_(cx, stringPrototype),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {}

        ICStub *getStub(ICStubSpace *space) {
            RootedShape stringProtoShape(cx, stringPrototype_->lastProperty());
            return ICGetProp_String::New(space, getStubCode(), firstMonitorStub_,
                                         stringProtoShape, offset_);
        }
    };
};

// Stub for accessing a string's length.
class ICGetProp_StringLength : public ICStub
{
    friend class ICStubSpace;

    ICGetProp_StringLength(IonCode *stubCode)
      : ICStub(GetProp_StringLength, stubCode)
    {}

  public:
    static inline ICGetProp_StringLength *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICGetProp_StringLength>(code);
    }

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::GetProp_StringLength)
        {}

        ICStub *getStub(ICStubSpace *space) {
            return ICGetProp_StringLength::New(space, getStubCode());
        }
    };
};

// Base class for GetProp_Native and GetProp_NativePrototype stubs.
class ICGetPropNativeStub : public ICMonitoredStub
{
    // Object shape (lastProperty).
    HeapPtrShape shape_;

    // Fixed or dynamic slot offset.
    uint32_t offset_;

  protected:
    ICGetPropNativeStub(ICStub::Kind kind, IonCode *stubCode, ICStub *firstMonitorStub,
                        HandleShape shape, uint32_t offset)
      : ICMonitoredStub(kind, stubCode, firstMonitorStub),
        shape_(shape),
        offset_(offset)
    {}

  public:
    HeapPtrShape &shape() {
        return shape_;
    }
    uint32_t offset() const {
        return offset_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICGetPropNativeStub, shape_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICGetPropNativeStub, offset_);
    }
};

// Stub for accessing an own property on a native object.
class ICGetProp_Native : public ICGetPropNativeStub
{
    friend class ICStubSpace;

    ICGetProp_Native(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                     uint32_t offset)
      : ICGetPropNativeStub(GetProp_Native, stubCode, firstMonitorStub, shape, offset)
    {}

  public:
    static inline ICGetProp_Native *New(ICStubSpace *space, IonCode *code,
                                        ICStub *firstMonitorStub, HandleShape shape,
                                        uint32_t offset)
    {
        return space->allocate<ICGetProp_Native>(code, firstMonitorStub, shape, offset);
    }
};

// Stub for accessing a property on a native object's prototype. Note that due to
// the shape teleporting optimization, we only have to guard on the object's shape
// and the holder's shape.
class ICGetProp_NativePrototype : public ICGetPropNativeStub
{
    friend class ICStubSpace;

    // Holder and its shape.
    HeapPtrObject holder_;
    HeapPtrShape holderShape_;

    ICGetProp_NativePrototype(IonCode *stubCode, ICStub *firstMonitorStub, HandleShape shape,
                              uint32_t offset, HandleObject holder, HandleShape holderShape)
      : ICGetPropNativeStub(GetProp_NativePrototype, stubCode, firstMonitorStub, shape, offset),
        holder_(holder),
        holderShape_(holderShape)
    {}

  public:
    static inline ICGetProp_NativePrototype *New(ICStubSpace *space, IonCode *code,
                                                 ICStub *firstMonitorStub, HandleShape shape,
                                                 uint32_t offset, HandleObject holder,
                                                 HandleShape holderShape)
    {
        return space->allocate<ICGetProp_NativePrototype>(code, firstMonitorStub, shape, offset,
                                                          holder, holderShape);
    }

  public:
    HeapPtrObject &holder() {
        return holder_;
    }
    HeapPtrShape &holderShape() {
        return holderShape_;
    }
    static size_t offsetOfHolder() {
        return offsetof(ICGetProp_NativePrototype, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICGetProp_NativePrototype, holderShape_);
    }
};

// Compiler for GetProp_Native and GetProp_NativePrototype stubs.
class ICGetPropNativeCompiler : public ICStubCompiler
{
    ICStub *firstMonitorStub_;
    HandleObject obj_;
    HandleObject holder_;
    bool isFixedSlot_;
    uint32_t offset_;

    bool generateStubCode(MacroAssembler &masm);

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
    }

  public:
    ICGetPropNativeCompiler(JSContext *cx, ICStub::Kind kind, ICStub *firstMonitorStub,
                            HandleObject obj, HandleObject holder, bool isFixedSlot,
                            uint32_t offset)
      : ICStubCompiler(cx, kind),
        firstMonitorStub_(firstMonitorStub),
        obj_(obj),
        holder_(holder),
        isFixedSlot_(isFixedSlot),
        offset_(offset)
    {}

    ICStub *getStub(ICStubSpace *space) {
        RootedShape shape(cx, obj_->lastProperty());
        if (kind == ICStub::GetProp_Native) {
            JS_ASSERT(obj_ == holder_);
            return ICGetProp_Native::New(space, getStubCode(), firstMonitorStub_, shape, offset_);
        }

        JS_ASSERT(obj_ != holder_);
        JS_ASSERT(kind == ICStub::GetProp_NativePrototype);
        RootedShape holderShape(cx, holder_->lastProperty());
        return ICGetProp_NativePrototype::New(space, getStubCode(), firstMonitorStub_, shape,
                                              offset_, holder_, holderShape);
    }
};

// SetProp
//     JSOP_SETPROP
//     JSOP_SETNAME
//     JSOP_SETGNAME
//     JSOP_INITPROP

class ICSetProp_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICSetProp_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::SetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICSetProp_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICSetProp_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::SetProp_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICSetProp_Fallback::New(space, getStubCode());
        }
    };
};

// Optimized SETPROP/SETGNAME/SETNAME stub.
class ICSetProp_Native : public ICUpdatedStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    HeapPtrTypeObject type_;
    HeapPtrShape shape_;
    uint32_t offset_;

    ICSetProp_Native(IonCode *stubCode, HandleTypeObject type, HandleShape shape, uint32_t offset)
      : ICUpdatedStub(SetProp_Native, stubCode),
        type_(type),
        shape_(shape),
        offset_(offset)
    {}

  public:
    static inline ICSetProp_Native *New(ICStubSpace *space, IonCode *code, HandleTypeObject type,
                                        HandleShape shape, uint32_t offset)
    {
        return space->allocate<ICSetProp_Native>(code, type, shape, offset);
    }
    HeapPtrTypeObject &type() {
        return type_;
    }
    HeapPtrShape &shape() {
        return shape_;
    }
    static size_t offsetOfType() {
        return offsetof(ICSetProp_Native, type_);
    }
    static size_t offsetOfShape() {
        return offsetof(ICSetProp_Native, shape_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICSetProp_Native, offset_);
    }

    class Compiler : public ICStubCompiler {
        HandleTypeObject type_;
        RootedShape shape_;
        bool isFixedSlot_;
        uint32_t offset_;

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isFixedSlot_) << 16);
        }

        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx, HandleTypeObject type, Shape *shape,
                 bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, ICStub::SetProp_Native),
            type_(type),
            shape_(cx, shape),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {}

        ICUpdatedStub *getStub(ICStubSpace *space) {
            ICUpdatedStub *stub = ICSetProp_Native::New(space, getStubCode(), type_, shape_, offset_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

// Call
//      JSOP_CALL
//      JSOP_FUNAPPLY
//      JSOP_FUNCALL
//      JSOP_NEW

class ICCallStubCompiler : public ICStubCompiler
{
  protected:
    ICCallStubCompiler(JSContext *cx, ICStub::Kind kind)
      : ICStubCompiler(cx, kind)
    { }

    void pushCallArguments(MacroAssembler &masm, GeneralRegisterSet regs, Register argcReg);
};

class ICCall_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;
    uint32_t isConstructing_;

    ICCall_Fallback(IonCode *stubCode, bool isConstructing)
      : ICMonitoredFallbackStub(ICStub::Call_Fallback, stubCode),
        isConstructing_(isConstructing ? 1 : 0)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static inline ICCall_Fallback *New(ICStubSpace *space, IonCode *code, bool isConstructing)
    {
        return space->allocate<ICCall_Fallback>(code, isConstructing);
    }

    bool isConstructing() const {
        return isConstructing_;
    }
    static size_t offsetOfIsConstructing() {
        return offsetof(ICCall_Fallback, isConstructing_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        bool isConstructing_;
        uint32_t returnOffset_;
        bool generateStubCode(MacroAssembler &masm);
        bool postGenerateStubCode(MacroAssembler &masm, Handle<IonCode *> code);

      public:
        Compiler(JSContext *cx, bool isConstructing)
          : ICCallStubCompiler(cx, ICStub::Call_Fallback),
            isConstructing_(isConstructing)
        { }

        ICStub *getStub(ICStubSpace *space) {
            ICCall_Fallback *stub = ICCall_Fallback::New(space, getStubCode(), isConstructing_);
            if (!stub || !stub->initMonitoringChain(cx, space))
                return NULL;
            return stub;
        }
    };
};

class ICCall_Scripted : public ICMonitoredStub
{
    friend class ICStubSpace;

    HeapPtrFunction callee_;

    ICCall_Scripted(IonCode *stubCode, ICStub *firstMonitorStub, HandleFunction callee)
      : ICMonitoredStub(ICStub::Call_Scripted, stubCode, firstMonitorStub),
        callee_(callee)
    { }

  public:
    static inline ICCall_Scripted *New(
            ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub, HandleFunction callee)
    {
        return space->allocate<ICCall_Scripted>(code, firstMonitorStub, callee);
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Scripted, callee_);
    }
    HeapPtrFunction &callee() {
        return callee_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        bool isConstructing_;
        RootedFunction callee_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isConstructing_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleFunction callee,
                 bool isConstructing)
          : ICCallStubCompiler(cx, ICStub::Call_Scripted),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            callee_(cx, callee)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICCall_Scripted::New(space, getStubCode(), firstMonitorStub_, callee_);
        }
    };
};

class ICCall_Native : public ICMonitoredStub
{
    friend class ICStubSpace;

    HeapPtrFunction callee_;

    ICCall_Native(IonCode *stubCode, ICStub *firstMonitorStub, HandleFunction callee)
      : ICMonitoredStub(ICStub::Call_Native, stubCode, firstMonitorStub),
        callee_(callee)
    { }

  public:
    static inline ICCall_Native *New(ICStubSpace *space, IonCode *code, ICStub *firstMonitorStub,
                                     HandleFunction callee)
    {
        return space->allocate<ICCall_Native>(code, firstMonitorStub, callee);
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Native, callee_);
    }
    HeapPtrFunction &callee() {
        return callee_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub *firstMonitorStub_;
        bool isConstructing_;
        RootedFunction callee_;
        bool generateStubCode(MacroAssembler &masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(kind) | (static_cast<int32_t>(isConstructing_) << 16);
        }

      public:
        Compiler(JSContext *cx, ICStub *firstMonitorStub, HandleFunction callee,
                 bool isConstructing)
          : ICCallStubCompiler(cx, ICStub::Call_Native),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            callee_(cx, callee)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICCall_Native::New(space, getStubCode(), firstMonitorStub_, callee_);
        }
    };
};

// Stub for performing a TableSwitch, updating the IC's return address to jump
// to whatever point the switch is branching to.
class ICTableSwitch : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    void **table_;
    int32_t min_;
    int32_t length_;
    void *defaultTarget_;

    ICTableSwitch(IonCode *stubCode, void **table,
                  int32_t min, int32_t length, void *defaultTarget)
      : ICStub(TableSwitch, stubCode), table_(table),
        min_(min), length_(length), defaultTarget_(defaultTarget)
    {}

  public:
    static inline ICTableSwitch *New(ICStubSpace *space, IonCode *code, void **table,
                                     int32_t min, int32_t length, void *defaultTarget) {
        return space->allocate<ICTableSwitch>(code, table, min, length, defaultTarget);
    }

    void fixupJumpTable(HandleScript script, BaselineScript *baseline);

    class Compiler : public ICStubCompiler {
        bool generateStubCode(MacroAssembler &masm);

        jsbytecode *pc_;

      public:
        Compiler(JSContext *cx, jsbytecode *pc)
          : ICStubCompiler(cx, ICStub::TableSwitch), pc_(pc)
        {}

        ICStub *getStub(ICStubSpace *space);
    };
};

// IC for constructing an iterator from an input value.
class ICIteratorNew_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorNew_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorNew_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorNew_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIteratorNew_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorNew_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorNew_Fallback::New(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in an iterator.
class ICIteratorMore_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorMore_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorMore_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorMore_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIteratorMore_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorMore_Fallback::New(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in a native iterator.
class ICIteratorMore_Native : public ICStub
{
    friend class ICStubSpace;

    ICIteratorMore_Native(IonCode *stubCode)
      : ICStub(ICStub::IteratorMore_Native, stubCode)
    { }

  public:
    static inline ICIteratorMore_Native *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIteratorMore_Native>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Native)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorMore_Native::New(space, getStubCode());
        }
    };
};

// IC for getting the next value in an iterator.
class ICIteratorNext_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorNext_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorNext_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorNext_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIteratorNext_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorNext_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorNext_Fallback::New(space, getStubCode());
        }
    };
};

// IC for getting the next value in a native iterator.
class ICIteratorNext_Native : public ICStub
{
    friend class ICStubSpace;

    ICIteratorNext_Native(IonCode *stubCode)
      : ICStub(ICStub::IteratorNext_Native, stubCode)
    { }

  public:
    static inline ICIteratorNext_Native *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIteratorNext_Native>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorNext_Native)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorNext_Native::New(space, getStubCode());
        }
    };
};

// IC for closing an iterator.
class ICIteratorClose_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICIteratorClose_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::IteratorClose_Fallback, stubCode)
    { }

  public:
    static inline ICIteratorClose_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICIteratorClose_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::IteratorClose_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICIteratorClose_Fallback::New(space, getStubCode());
        }
    };
};

// InstanceOf
//      JSOP_INSTANCEOF
class ICInstanceOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICInstanceOf_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::InstanceOf_Fallback, stubCode)
    { }

  public:
    static inline ICInstanceOf_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICInstanceOf_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::InstanceOf_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICInstanceOf_Fallback::New(space, getStubCode());
        }
    };
};

// TypeOf
//      JSOP_TYPEOF
//      JSOP_TYPEOFEXPR
class ICTypeOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    ICTypeOf_Fallback(IonCode *stubCode)
      : ICFallbackStub(ICStub::TypeOf_Fallback, stubCode)
    { }

  public:
    static inline ICTypeOf_Fallback *New(ICStubSpace *space, IonCode *code) {
        return space->allocate<ICTypeOf_Fallback>(code);
    }

    class Compiler : public ICStubCompiler {
      protected:
        bool generateStubCode(MacroAssembler &masm);

      public:
        Compiler(JSContext *cx)
          : ICStubCompiler(cx, ICStub::TypeOf_Fallback)
        { }

        ICStub *getStub(ICStubSpace *space) {
            return ICTypeOf_Fallback::New(space, getStubCode());
        }
    };
};

} // namespace ion
} // namespace js

#endif

