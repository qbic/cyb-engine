#pragma once
#include <string>
#include <variant>
#include <functional>
#include "core/enum_flags.h"
#include "core/types.h"

namespace cyb
{
    using CVarValue = std::variant<int32_t, uint32_t, float, bool, std::string>;
    using CVarCallback = std::function<void(const CVarValue&)>;

    enum class CVarFlag : uint32_t
    {
        SystemBit   = BIT(0),
        RendererBit = BIT(1),
        GuiBit      = BIT(2),
        GameBit     = BIT(3),
        RomBit      = BIT(10),          // read only access, cannot be changed by user
        NoSave      = BIT(11),          // cvar wont be written during serialization
        ModifiedBit = BIT(12)
    };
    CYB_ENABLE_BITMASK_OPERATORS(CVarFlag);

    class CVar
    {
    public:
        CVar(const std::string_view& name, const CVarValue& value, CVarFlag flags, const std::string_view& description);

        // if variant type is constructable by T, value will be implicitly cast
        template <typename T>
        void SetValue(const T v)
        {
            std::visit([&]<typename U>(U& current) {
                if constexpr (std::is_constructible_v<std::decay_t<U>, T>)
                {
                    SetValueImpl(CVarValue(static_cast<std::decay_t<U>>(v)));
                    return;
                }

                // T is non-constuctable type of U and
                // this call will bail out, generating a warning
                SetValueImpl(CVarValue(v));
            }, m_value);
        }

        void Update();

        // returns T() on type mismatch
        template <typename T>
        [[nodiscard]] T GetValue() const
        {
            const T* v = std::get_if<T>(&m_value);
            return (v != nullptr) ? *v : T();
        }

        [[nodiscard]] const CVarValue& GetVariant() const;
        [[nodiscard]] const std::string& GetValueAsString() const;
        [[nodiscard]] const std::string GetTypeAsString() const;
        [[nodiscard]] const std::string& GetName() const;
        [[nodiscard]] const std::string& GetDescription() const;
        [[nodiscard]] bool IsModified() const;
        void SetModified();
        void ClearModified();
        void SetOnChangeCallback(const CVarCallback& callback);

        static void RegisterStaticCVars();

    private:
        void SetValueImpl(const CVarValue& value);

        const std::string m_name;
        const std::string m_description;
        CVarValue m_value;
        std::string m_valueAsString;        // only used for non-string types
        size_t m_typeIndex;
        CVarFlag m_flags;
        CVarCallback m_onChangeCallback;
    };
}

namespace cyb::cvar_system
{
    void Register(CVar* cvar);
    [[nodiscard]] CVar* Find(const std::string_view& name);
    [[nodiscard]] const std::unordered_map<std::string_view, CVar*>& GetRegistry();
}