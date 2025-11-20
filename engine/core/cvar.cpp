#include <cassert>
#include "core/cvar.h"
#include "core/hash.h"
#include "core/logger.h"

namespace cyb
{
    CVarRegistryMapType g_cvarRegistry;

    // This registry is for pre RegisterStaticCVars() only.
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

    namespace detail
    {
        CVarBase::CVarBase(const std::string& name, CVarFlag flags, const std::string& description) :
            m_name(name),
            m_flags(flags),
            m_description(description),
            m_hash(HashString(name))
        {
            if (!IsStaticCVarsRegistered())
                StaticRegistry().push_back(this);
            else
                RegisterCVar(this);
        }

        uint64_t CVarBase::GetHash() const
        {
            return m_hash;
        }

        const std::string& CVarBase::GetName() const
        {
            return m_name;
        }

        const std::string& CVarBase::GetDescription() const
        {
            return m_description;
        }

        bool CVarBase::IsModified() const
        {
            return HasFlag(m_flags, CVarFlag::ModifiedBit);
        }

        void CVarBase::SetModified(bool value)
        {
            SetFlag(m_flags, CVarFlag::ModifiedBit, value);
        }

        bool CVarBase::IsReadOnly() const
        {
            return HasFlag(m_flags, CVarFlag::RomBit);
        }
    }

    const CVarRegistryMapType& GetCVarRegistry()
    {
        return g_cvarRegistry;
    }
} // cyb::detail