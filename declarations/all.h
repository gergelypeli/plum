
#include "declaration.h"
#include "scope.h"
#include "util.h"
#include "identifier.h"
#include "allocable.h"
#include "function.h"
#include "associable.h"
#include "type.h"

#include "interface.h"
#include "basic.h"
#include "float.h"
#include "reference.h"
#include "record.h"
#include "class.h"
#include "option.h"
#include "container.h"
#include "nosy.h"

// Types

class VirtualEntry;

// Declarations
class Declaration;
class Identifier;
class Allocable;
class Associable;
class Variable;
class PartialVariable;
class GlobalVariable;
class Role;
class Evaluable;
class Function;
class SysvFunction;
class ImportedFloatFunction;
class ClassType;

// Types
class Type;
class MetaType;
class TreenumerationType;
class ArrayType;
class InterfaceType;
class AbstractType;
class Implementation;

// Scopes
class Scope;
class CodeScope;
class DataScope;
class ModuleScope;
class SwitchScope;
class TryScope;
class EvalScope;
class RetroScope;
class FunctionScope;
class ExportScope;
class RootScope;

extern const std::string MAIN_ROLE_NAME;
extern const std::string BASE_ROLE_NAME;
extern const std::string QUALIFIER_NAME;

