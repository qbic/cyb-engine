#include <cassert>
#include "core/cvar.h"
#include "core/hash.h"
#include "core/logger.h"

namespace cyb {

CVarRegistryMapType g_cvarRegistry;

// This registry is for pre-RegisterStaticCVars() only.
static std::vector<detail::CVarBase*>& StaticRegistry()
{
    static std::vector<detail::CVarBase*> registry;
    return registry;
}

static bool& IsStaticCVarsRegistered()
{
    static bool registered = false;
    return registered;
}

static void RegisterCVar(detail::CVarBase* cvar)
{
    assert(cvar != nullptr);
    const auto& [_, inserted] = g_cvarRegistry.insert({ cvar->GetHash(), cvar });
    if (!inserted)
    {
        CYB_WARNING("RegisterCVar(): '{}' allready exist", cvar->GetName());
        return;
    }

    CYB_TRACE("Registered CVar '{}' [Type: {}] with value '{}'", cvar->GetName(), cvar->GetTypeAsString(), cvar->GetValueAsString());
}

void RegisterStaticCVars()
{
    if (IsStaticCVarsRegistered())
        return;

    for (auto cvar : StaticRegistry())
        RegisterCVar(cvar);

    IsStaticCVarsRegistered() = true;
    StaticRegistry().clear();
}

//------------------------------------------------------------------------------

namespace detail {

CVarBase::CVarBase(const std::string& name, CVarFlag flags, const std::string& description) :
    name_(name),
    flags_(flags),
    description_(description),
    hash_(HashString(name))
{
    if (!IsStaticCVarsRegistered())
        StaticRegistry().push_back(this);
    else
        RegisterCVar(this);
}

uint64_t CVarBase::GetHash() const
{
    return hash_;
}

const std::string& CVarBase::GetName() const
{
    return name_;
}

const std::string& CVarBase::GetDescription() const
{
    return description_;
}

bool CVarBase::IsModified() const
{
    return HasFlag(flags_, CVarFlag::ModifiedBit);
}

void CVarBase::SetModified(bool value)
{
    SetFlag(flags_, CVarFlag::ModifiedBit, value);
}

bool CVarBase::IsReadOnly() const
{
    return HasFlag(flags_, CVarFlag::RomBit);
}
}

const CVarRegistryMapType& GetCVarRegistry()
{
    return g_cvarRegistry;
}

} // cyb::detaul