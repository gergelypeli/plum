#ifndef PLUM_H
#define PLUM_H

// Standard libraries
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>

#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>

// Dependency libraries
#include <elf.h>
#include <libdwarf/dwarf.h>

// Shared files between the compiler and the runtime, in C
extern "C" {
#include "environment/shared.h"
}

// Low level stuff
#include "arch/all.h"

// Typename declarations
class Scope;
class DataScope;
class RootScope;
class CodeScope;
class ModuleScope;
class FunctionScope;
class SwitchScope;
class TryScope;
class EvalScope;
class RetroScope;

class Declaration;
class Type;
class MetaType;
class TreenumerationType;
class InterfaceType;
class AbstractType;
class Associable;
class Function;
class Variable;
class GlobalVariable;

class Value;
class DeclarationValue;
class DataBlockValue;
class YieldableValue;

class VirtualEntry;
class AutoconvEntry;
class SelfInfo;

// Regular headers
#include "util.h"
#include "globals/all.h"
#include "parsing/all.h"
#include "declarations/all.h"
#include "values/all.h"

// Defined here
ModuleScope *import_module(std::string required_name, Scope *scope);
std::string get_source_file_display_name(int index);

#endif
