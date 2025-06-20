#include "core/cvar.h"
#include "core/hash.h"
#include "core/logger.h"

namespace cyb::cvar
{
    Registry cvarRegistry;

    static std::vector<CVar*>& StaticRegistry()
    {
        static std::vector<CVar*> registry;
        return registry;
    }

    static bool& IsStaticCVarsRegistered()
    {
        static bool registered = false;
        return registered;
    }

    CVar::CVar(const std::string_view& name, const Value& value, Flag flags, const std::string_view& description) :
        m_name(name),
        m_value(value),
        m_typeIndex(value.index()),
        m_flags(flags),
        m_description(description)
    {
        Update();

        if (!IsStaticCVarsRegistered())
            StaticRegistry().push_back(this);
        else
            cvar::Register(this);
    }

    void CVar::SetValueImpl(const Value& value)
    {
        if (m_value.index() != value.index())
        {
            CYB_WARNING("Type mismatch when setting CVar: {}", GetName());
            return;
        }

        if (HasFlag(m_flags, Flag::RomBit))
        {
            CYB_WARNING("Cannot change value of read-only CVar: {}", GetName());
            return;
        }

        if (m_value == value)
            return;     // same value

        m_value = value;
        Update();
        SetFlag(m_flags, Flag::ModifiedBit, true);

        if (m_onChangeCallback)
            m_onChangeCallback(this);
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
            else if constexpr (std::is_same_v<T, float>)
                return std::format("{:.2f}", v);
            else
                return std::to_string(v);
        }, m_value);

        m_hash = hash::String(GetName());
    }

    const CVar::Value& CVar::GetVariant() const
    {
        return m_value;
    }

    const std::string& CVar::GetValueAsString() const
    {
        return std::visit([&] (auto&& v) -> const std::string& {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>)
                return v;
            return m_valueAsString;
        }, m_value);
    }

    const std::string CVar::GetTypeAsString() const
    {
        return std::visit([] (auto&& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int32_t>) return "int";
            if constexpr (std::is_same_v<T, uint32_t>) return "uint";
            if constexpr (std::is_same_v<T, float>) return "float";
            if constexpr (std::is_same_v<T, bool>) return "bool";
            if constexpr (std::is_same_v<T, std::string>) return "string";
            return "unknown";
        }, m_value);
    }

    const std::string& CVar::GetName() const 
    {
        return m_name;
    }

    const std::string& CVar::GetDescription() const
    {
        return m_description;
    }

    const uint64_t CVar::GetHash() const
    {
        return m_hash;
    }

    bool CVar::IsModified() const
    {
        return HasFlag(m_flags, Flag::ModifiedBit);
    }

    void CVar::SetModified()
    {
        SetFlag(m_flags, Flag::ModifiedBit, true);
    }
    
    void CVar::ClearModified()
    {
        SetFlag(m_flags, Flag::ModifiedBit, false);
    }
    
    void CVar::SetOnChangeCallback(const Callback& callback)
    {
        m_onChangeCallback = callback;
    }

    void CVar::RegisterStaticCVars()
    {
        if (IsStaticCVarsRegistered())
            return;

        for (CVar* cvar : StaticRegistry())
            cvar::Register(cvar);

        IsStaticCVarsRegistered() = true;
        StaticRegistry().clear();
    }

    void Register(CVar* cvar)
    {
        assert(cvar != nullptr);
        const auto& [_, inserted] = cvarRegistry.insert({ cvar->GetHash(), cvar });
        if (!inserted)
        {
            CYB_WARNING("cvar::Register(): '{}' allready exist", cvar->GetName());
            return;
        }

        CYB_TRACE("Registered CVar '{}' [Type: {}] with value '{}'", cvar->GetName(), cvar->GetTypeAsString(), cvar->GetValueAsString());
    }

    CVar* Find(const std::string_view& name)
    {
        uint64_t hash = hash::String(name);
        auto it = cvarRegistry.find(hash);
        return (it != cvarRegistry.end()) ? it->second : nullptr;
    }

    const Registry& GetRegistry()
    {
        return cvarRegistry;
    }
}