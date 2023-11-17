#ifndef ENGINE_BYTECODE_TYPES_H
#define ENGINE_BYTECODE_TYPES_H

#include <Engine/Includes/HashMap.h>
#include <Engine/IO/Stream.h>

#define FRAMES_MAX 64
#define STACK_SIZE_MAX (FRAMES_MAX * 256)
#define THREAD_NAME_MAX 64

typedef enum {
    ERROR_RES_EXIT,
    ERROR_RES_CONTINUE,
} ErrorResult;

typedef enum {
    VAL_NULL,
    VAL_INTEGER,
    VAL_DECIMAL,
    VAL_OBJECT,
    VAL_LINKED_INTEGER,
    VAL_LINKED_DECIMAL,
} ValueType;

struct Obj;

struct VMValue {
    Uint32    Type;
    union {
        int    Integer;
        float  Decimal;
        Obj*   Object;
        int*   LinkedInteger;
        float* LinkedDecimal;
    } as;
};

struct Chunk {
    int              Count;
    int              Capacity;
    Uint8*           Code;
    Uint8*           Failsafe;
    int*             Lines;
    vector<VMValue>* Constants;
    bool             OwnsMemory;
};

struct Bytecode {
    Uint8* Data;
    size_t Size;
};

struct PrintBuffer {
    char** Buffer;
    int WriteIndex;
    int BufferSize;
};
int buffer_printf(PrintBuffer* printBuffer, const char *format, ...);

void ChunkInit(Chunk* chunk);
void ChunkFree(Chunk* chunk);
void ChunkWrite(Chunk* chunk, Uint8 byte, int line);
int  ChunkAddConstant(Chunk* chunk, VMValue value);

const char* GetTypeString(VMValue value);

#define IS_NULL(value)  ((value).Type == VAL_NULL)
#define IS_INTEGER(value)  ((value).Type == VAL_INTEGER)
#define IS_DECIMAL(value)  ((value).Type == VAL_DECIMAL)
#define IS_OBJECT(value)   ((value).Type == VAL_OBJECT)

#define AS_INTEGER(value)  (value.Type == VAL_INTEGER ? (value).as.Integer : *((value).as.LinkedInteger))
#define AS_DECIMAL(value)  (value.Type == VAL_DECIMAL ? (value).as.Decimal : *((value).as.LinkedDecimal))
#define AS_OBJECT(value)   ((value).as.Object)

#ifdef WIN32
    #define NULL_VAL           (VMValue { })
    static inline VMValue INTEGER_VAL(int value) { VMValue val; val.Type = VAL_INTEGER; val.as.Integer = value; return val; }
    static inline VMValue DECIMAL_VAL(float value) { VMValue val; val.Type = VAL_DECIMAL; val.as.Decimal = value; return val; }
    static inline VMValue OBJECT_VAL(void* value) { VMValue val; val.Type = VAL_OBJECT; val.as.Object = (Obj*)value; return val; }
    static inline VMValue INTEGER_LINK_VAL(int* value) { VMValue val; val.Type = VAL_LINKED_INTEGER; val.as.LinkedInteger = value; return val; }
    static inline VMValue DECIMAL_LINK_VAL(float* value) { VMValue val; val.Type = VAL_LINKED_DECIMAL; val.as.LinkedDecimal = value; return val; }
#else
    #define NULL_VAL           ((VMValue) { VAL_NULL, { .Integer = 0 } })
    #define INTEGER_VAL(value) ((VMValue) { VAL_INTEGER, { .Integer = value } })
    #define DECIMAL_VAL(value) ((VMValue) { VAL_DECIMAL, { .Decimal = value } })
    #define OBJECT_VAL(object) ((VMValue) { VAL_OBJECT, { .Object = (Obj*)object } })
    #define INTEGER_LINK_VAL(value)  ((VMValue) { VAL_LINKED_INTEGER, { .LinkedInteger = value } })
    #define DECIMAL_LINK_VAL(value)  ((VMValue) { VAL_LINKED_DECIMAL, { .LinkedDecimal = value } })
#endif

#define IS_LINKED_INTEGER(value) ((value).Type == VAL_LINKED_INTEGER)
#define IS_LINKED_DECIMAL(value) ((value).Type == VAL_LINKED_DECIMAL)
#define AS_LINKED_INTEGER(value)  (*((value).as.LinkedInteger))
#define AS_LINKED_DECIMAL(value)  (*((value).as.LinkedDecimal))

// NOTE: Engine can either use integer or decimal for the number value.
//   Set this to integer for Sonic, and decimal for non-optimized, floating-point projects.
// #define IE_FIXED_POINT_MATH
#ifdef  IE_FIXED_POINT_MATH
    #define IS_NUMBER(value)        (IS_INTEGER(value))
    #define IS_LINKED_NUMBER(value) (IS_LINKED_INTEGER(value))
    #define AS_NUMBER(value)        (AS_INTEGER(value))
    #define AS_LINKED_NUMBER(value) (AS_LINKED_INTEGER(value))
    #define NUMBER_VAL(value)       (INTEGER_VAL(value))
    #define VAL_NUMBER              VAL_INTEGER
    #define VAL_LINKED_NUMBER       VAL_LINKED_INTEGER
    #define NUMBER_CTYPE            int
#else
    #define IS_NUMBER(value)        (IS_DECIMAL(value))
    #define IS_LINKED_NUMBER(value) (IS_LINKED_DECIMAL(value))
    #define AS_NUMBER(value)        (AS_DECIMAL(value))
    #define AS_LINKED_NUMBER(value) (AS_LINKED_DECIMAL(value))
    #define NUMBER_VAL(value)       (DECIMAL_VAL(value))
    #define VAL_NUMBER              VAL_DECIMAL
    #define VAL_LINKED_NUMBER       VAL_LINKED_DECIMAL
    #define NUMBER_CTYPE            float
#endif

typedef VMValue (*NativeFn)(int argCount, VMValue* args, Uint32 threadID);


#define OBJECT_TYPE(value)      (AS_OBJECT(value)->Type)
#define IS_BOUND_METHOD(value)  IsObjectType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)         IsObjectType(value, OBJ_CLASS)
#define IS_CLOSURE(value)       IsObjectType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)      IsObjectType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)      IsObjectType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)        IsObjectType(value, OBJ_NATIVE)
#define IS_STRING(value)        IsObjectType(value, OBJ_STRING)
#define IS_ARRAY(value)         IsObjectType(value, OBJ_ARRAY)
#define IS_MAP(value)           IsObjectType(value, OBJ_MAP)
#define IS_STREAM(value)        IsObjectType(value, OBJ_STREAM)

#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJECT(value))
#define AS_CLASS(value)         ((ObjClass*)AS_OBJECT(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJECT(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJECT(value))
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJECT(value))->Function)
#define AS_STRING(value)        ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJECT(value))->Chars)
#define AS_ARRAY(value)         ((ObjArray*)AS_OBJECT(value))
#define AS_MAP(value)           ((ObjMap*)AS_OBJECT(value))
#define AS_STREAM(value)        ((ObjStream*)AS_OBJECT(value))

enum ObjType {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_STREAM
};

typedef HashMap<VMValue> Table;

struct Obj {
    ObjType          Type;
    bool             IsDark;
    struct ObjClass* Class;
    struct Obj*      Next;
};
struct ObjString {
    Obj    Object;
    size_t Length;
    char*  Chars;
    Uint32 Hash;
};
struct ObjFunction {
    Obj          Object;
    int          Arity;
    int          UpvalueCount;
    struct Chunk Chunk;
    size_t       FunctionListOffset;
    ObjString*   Name;
    ObjString*   ClassName;
    char         SourceFilename[256];
    Uint32       NameHash;
};
struct ObjNative {
    Obj      Object;
    NativeFn Function;
};
struct ObjUpvalue {
    Obj      Object;
    VMValue* Value;
    VMValue  Closed;
    struct ObjUpvalue* Next;
};
struct ObjClosure {
    Obj          Object;
    ObjFunction* Function;
    ObjUpvalue** Upvalues;
    int          UpvalueCount;
};
struct ObjClass {
    Obj        Object;
    ObjString* Name;
    Uint32     Hash;
    Table*     Methods;
    Table*     Fields; // Keep this as a pointer, so that a new table isn't created when passing an ObjClass value around
    VMValue    Initializer;
    Uint8      Extended;
    Uint32     ParentHash;
    ObjClass*  Parent;
};
struct ObjInstance {
    Obj       Object;
    Table*    Fields;
    void*     EntityPtr;
};
struct ObjBoundMethod {
    Obj          Object;
    VMValue      Receiver;
    ObjFunction* Method;
};
struct ObjArray {
    Obj              Object;
    vector<VMValue>* Values;
};
struct ObjMap {
    Obj               Object;
    HashMap<VMValue>* Values;
    HashMap<char*>*   Keys;
};
struct ObjStream {
    Obj               Object;
    Stream*           StreamPtr;
    bool              Writable;
    bool              Closed;
};

ObjString*         TakeString(char* chars, size_t length);
ObjString*         TakeString(char* chars);
ObjString*         CopyString(const char* chars, size_t length);
ObjString*         CopyString(const char* chars);
ObjString*         AllocString(size_t length);
char*              HeapCopyString(const char* str, size_t len);
ObjFunction*       NewFunction();
ObjNative*         NewNative(NativeFn function);
ObjUpvalue*        NewUpvalue(VMValue* slot);
ObjClosure*        NewClosure(ObjFunction* function);
ObjClass*          NewClass(Uint32 hash);
ObjInstance*       NewInstance(ObjClass* klass);
ObjBoundMethod*    NewBoundMethod(VMValue receiver, ObjFunction* method);
ObjArray*          NewArray();
ObjMap*            NewMap();
ObjStream*         NewStream(Stream* streamPtr, bool writable);

#define FREE_OBJ(obj, type) \
    assert(GarbageCollector::GarbageSize >= sizeof(type)); \
    GarbageCollector::GarbageSize -= sizeof(type); \
    Memory::Free(obj)

bool               ValuesEqual(VMValue a, VMValue b);

static inline bool IsObjectType(VMValue value, ObjType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->Type == type;
}

static inline bool HasInitializer(ObjClass* klass) {
    return !IS_NULL(klass->Initializer);
}

struct WithIter {
    void* entity;
    void* entityNext;
    int   index;
    void* registry;
    Uint8 receiverSlot;
};

struct CallFrame {
    ObjFunction* Function;
    Uint8*       IP;
    Uint8*       IPLast;
    Uint8*       IPStart;
    VMValue*     Slots;
    int          FunctionListOffset;

    VMValue   WithReceiverStack[16];
    VMValue*  WithReceiverStackTop = WithReceiverStack;
    WithIter  WithIteratorStack[16];
    WithIter* WithIteratorStackTop = WithIteratorStack;
};
enum   OpCode {
    OP_ERROR = 0,
    OP_CONSTANT,
    // Classes and Instances
    OP_DEFINE_GLOBAL,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    //
    OP_PRINT_STACK,
    //
    OP_INHERIT,
    OP_RETURN,
    OP_METHOD,
    OP_CLASS,
    // Function Operations
    OP_CALL,
    OP_SUPER,
    OP_INVOKE,
    // Jumping
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_BACK,
    // Stack Operation
    OP_POP,
    OP_COPY,
    // OP_SAVE_VALUE,
    // OP_LOAD_VALUE,
    // Numeric Operations
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_NEGATE,
    OP_INCREMENT,
    OP_DECREMENT,
    // Bit Operations
    OP_BITSHIFT_LEFT,
    OP_BITSHIFT_RIGHT,
    // Constants
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    // Bitwise Operations
    OP_BW_NOT,
    OP_BW_AND,
    OP_BW_OR,
    OP_BW_XOR,
    // Logical Operations
    OP_LG_NOT,
    OP_LG_AND,
    OP_LG_OR,
    // Equality and Comparison Operators
    OP_EQUAL,
    OP_EQUAL_NOT,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    //
    OP_PRINT,
    OP_ENUM,
    OP_SAVE_VALUE,
    OP_LOAD_VALUE,
    OP_WITH,
    OP_GET_ELEMENT,
    OP_SET_ELEMENT,
    OP_NEW_ARRAY,
    OP_NEW_MAP,
    //
    OP_SWITCH_TABLE,
    OP_FAILSAFE,
    OP_EVENT,
    OP_TYPEOF,
    OP_NEW,
    OP_IMPORT,
    OP_SWITCH,
    OP_POPN,
    OP_HAS_PROPERTY,
    OP_IMPORT_MODULE,

    OP_SYNC = 0xFF,
};

#endif /* ENGINE_BYTECODE_TYPES_H */
