#pragma once
#include <string>
#include <variant>
#include <functional>
#include "core/enum_flags.h"
#include "core/non_copyable.h"
#include "core/types.h"

namespace cyb::cvar
{
    enum class Flag : uint32_t
    {
        SystemBit = BIT(0),
        RendererBit = BIT(1),
        GuiBit = BIT(2),
        GameBit = BIT(3),
        RomBit = BIT(10),           // read only access, cannot be changed by user
        NoSaveBit = BIT(11),        // cvar wont be written during serialization
        ModifiedBit = BIT(12)
    };
    CYB_ENABLE_BITMASK_OPERATORS(Flag);

    class CVar : private NonCopyable
    {
    public:
        using Value = std::variant<int32_t, uint32_t, float, bool, std::string>;
        using Callback = std::function<void(const CVar*)>;

        CVar() = delete;
        explicit CVar(const std::string_view& name, const Value& value, Flag flags, const std::string_view& description);

        // if variant type is constructable by T, value will be implicitly cast
        template <typename T>
        void SetValue(const T v)
        {
            std::visit([&]<typename U>(U& current) {
                if constexpr (std::is_constructible_v<std::decay_t<U>, T>)
                {
                    SetValueImpl(Value(static_cast<std::decay_t<U>>(v)));
                    return;
                }

                // T is non-constuctable type of U and
                // this call will bail out, generating a warning
                SetValueImpl(Value(v));
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

        [[nodiscard]] const Value& GetVariant() const;
        [[nodiscard]] const std::string& GetValueAsString() const;
        [[nodiscard]] const std::string GetTypeAsString() const;
        [[nodiscard]] const std::string& GetName() const;
        [[nodiscard]] const std::string& GetDescription() const;
        [[nodiscard]] const uint64_t GetHash() const;
        [[nodiscard]] bool IsModified() const;
        void SetModified();
        void ClearModified();
        void SetOnChangeCallback(const Callback& callback);

        // this need's to be called once on initialization to register
        // all globally declared cvars
        static void RegisterStaticCVars();

    private:
        void SetValueImpl(const Value& value);

        const std::string m_name;
        const std::string m_description;
        uint64_t m_hash = 0;
        Value m_value;
        std::string m_valueAsString;        // only used for non-string types
        size_t m_typeIndex = 0;
        Flag m_flags;
        Callback m_onChangeCallback;
    };

    using Registry = std::unordered_map<uint64_t, CVar*>;

    void Register(CVar* cvar);

    // finding cvar using hash enables compile time hashing, usage:
    //  auto* cvar = Find(hash::String("cvarName"));
    [[nodiscard]] CVar* Find(const uint64_t hash);
    [[nodiscard]] CVar* Find(const std::string_view& name);
    [[nodiscard]] const Registry& GetRegistry();
}