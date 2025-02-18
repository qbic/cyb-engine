#include "core/cvar.h"
#include "core/logger.h"

#define STATIC_CVARS_REGISTERED  (CVar*)std::numeric_limits<ptrdiff_t>::max()

namespace cyb
{
    CVar* CVar::staticCVars = nullptr;

    CVar::CVar(const std::string_view& name, const CVarValue& value, CVarFlag flags, const std::string_view& descrition) :
        m_name(name),
        m_value(value),
        m_typeIndex(value.index()),
        m_flags(flags),
        m_description(descrition),
        m_next(nullptr)
    {
        Update();

        if (staticCVars != STATIC_CVARS_REGISTERED)
        {
            this->m_next = staticCVars;
            staticCVars = this;
        }
        else
        {
            cvar_system::Register(this);
        }
    }

    void CVar::SetValue(const CVarValue& value)
    {
        if (m_value.index() != CVarValue(value).index())
        {
            CYB_WARNING("Trying to set CVar '{}' to invalid type", GetName());
            return;
        }

        if (HasFlag(m_flags, CVarFlag::RomBit))
        {
            CYB_WARNING("Cannot change the value of read-only CVar '{}'", GetName());
            return;
        }

        m_value = value;
        Update();
        SetFlag(m_flags, CVarFlag::ModifiedBit, true);

        if (m_onChangeCallback)
            m_onChangeCallback(m_value);
    }

    void CVar::Update()
    {
        // string type cvars will use value stored in variant to get
        // the string value
        m_valueAsString = std::visit([] (auto&& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
                return "";
            else if constexpr (std::is_same_v<T, bool>)
                return v ? "true" : "false";
            else
                return std::to_string(v);
        }, m_value);
    }

    const std::string_view CVar::GetValueAsString() const
    {
        return std::visit([&] (auto&& v) -> const std::string_view {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>)
                return v;
            else
                return m_valueAsString;
        }, m_value);
    }

    const std::string_view& CVar::GetName() const 
    {
        return m_name;
    }

    bool CVar::IsModified() const
    {
        return HasFlag(m_flags, CVarFlag::ModifiedBit);
    }

    void CVar::SetModified()
    {
        SetFlag(m_flags, CVarFlag::ModifiedBit, true);
    }
    
    void CVar::ClearModified()
    {
        SetFlag(m_flags, CVarFlag::ModifiedBit, false);
    }
    
    void CVar::SetOnChangeCallback(const CVarCallback& callback)
    {
        m_onChangeCallback = callback;
    }

    void CVar::RegisterStaticCVars()
    {
        if (staticCVars != STATIC_CVARS_REGISTERED)
        {
            for (CVar* cvar = staticCVars; cvar; cvar = cvar->m_next)
                cvar_system::Register(cvar);
            staticCVars = STATIC_CVARS_REGISTERED;
        }
    }
}

namespace cyb::cvar_system
{
    std::unordered_map<std::string_view, CVar*> cvarRegistry;

    void Register(CVar* cvar)
    {
        if (Find(cvar->GetName()))
        {
            CYB_WARNING("CVar '{}' allready exist", cvar->GetName());
            return;
        }

        CYB_TRACE("Registered CVar '{}' with value '{}'", cvar->GetName(), cvar->GetValueAsString());
        cvarRegistry[cvar->GetName()] = cvar;
    }

    CVar* Find(const std::string_view& name)
    {
        auto it = cvarRegistry.find(name);
        return (it != cvarRegistry.end()) ? it->second : nullptr;
    }
}