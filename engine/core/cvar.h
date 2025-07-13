#pragma once
#include <cassert>
#include <string>
#include <format>
#include <functional>
#include "core/enum_flags.h"
#include "core/non_copyable.h"
#include "core/types.h"

namespace cyb
{
    enum class CVarFlag : uint32_t
    {
        SystemBit = BIT(0),
        RendererBit = BIT(1),
        GuiBit = BIT(2),
        GameBit = BIT(3),
        RomBit = BIT(10),           // read only access, cannot be changed by user
        NoSaveBit = BIT(11),        // cvar wont be written during serialization
        ModifiedBit = BIT(12)
    };
    CYB_ENABLE_BITMASK_OPERATORS(CVarFlag);

    template <typename T>
    class CVar;

    namespace detail
    {
        class CVarBase : private NonCopyable
        {
        public:
            explicit CVarBase(const std::string& name, CVarFlag flags, const std::string& description);
            
            [[nodiscard]] uint64_t GetHash() const;
            [[nodiscard]] const std::string& GetName() const;
            [[nodiscard]] const std::string& GetDescription() const;
            [[nodiscard]] bool IsModified() const;
            void SetModified(bool value);
            [[nodiscard]] bool IsReadOnly() const;

            virtual const std::type_info& GetType() const = 0;
            virtual const std::string& GetValueAsString() const = 0;
            virtual const std::string& GetTypeAsString() const = 0;

        private:
            const std::string m_name;
            const std::string m_description;
            uint64_t m_hash;                // computed from name
            CVarFlag m_flags;
        };

        template<typename T>
        class CVarCommon : public CVarBase
        {
        public:
            using ValueType = T;
            using CallbackType = std::function<void(const CVar<T>&)>;

            explicit CVarCommon(const std::string& name, ValueType value, CVarFlag flags, const std::string& description) :
                CVarBase(name, flags, description),
                m_value(value)
            {
            }

            void SetValue(const ValueType& value)
            {
                if (IsReadOnly() || m_value == value)
                    return;

                m_value = value;
                OnModifyValue();
                SetModified(true);
                RunOnChangeCallbacks();
            }

            [[nodiscard]] const std::type_info& GetType() const override
            {
                return typeid(T);
            }

            [[nodiscard]] const ValueType& GetValue() const
            {
                return m_value;
            }

            void RunOnChangeCallbacks() const
            {
                for (auto& callback : m_callbacks)
                    callback(*static_cast<const CVar<T>*>(this));
            }

            void RegisterOnChangeCallback(const CallbackType& callback)
            {
                m_callbacks.push_back(callback);
            }
            
        private:
            virtual void OnModifyValue()
            {
            }

            ValueType m_value;
            std::string m_valueString;
            std::vector<CallbackType> m_callbacks;
        };
    }

    template<typename T>
    concept IsNumberFamily = std::same_as<T, int32_t> ||
                             std::same_as<T, uint32_t> ||
                             std::same_as<T, float>;

    template<IsNumberFamily T>
    class CVar<T> : public detail::CVarCommon<T>
    {
    public:
        explicit CVar(const std::string& name, const T value, CVarFlag flags, const std::string& description) :
            detail::CVarCommon<T>(name, value, flags, description)
        {
            OnModifyValue();
        }

        [[nodiscard]] const std::string& GetValueAsString() const
        {
            return m_valueString;
        }

        [[nodiscard]] const std::string& GetTypeAsString() const override
        {
            static const std::string signedIntString = "signed int";
            static const std::string unsignedIntString = "unsigned int";
            static const std::string floatString = "float";
            static const std::string emptyString = "";

            if constexpr (std::is_same_v<T, int32_t>)
                return signedIntString;
            else if constexpr (std::is_same_v<T, uint32_t>)
                return unsignedIntString;
            else if constexpr (std::is_same_v<T, float>)
                return floatString;

            assert(0);
            return emptyString;
        }

    private:
        void OnModifyValue() override
        {
            if constexpr (std::is_same_v<T, float>)
                m_valueString = std::format("{:.2f}", this->GetValue());
            else
                m_valueString = std::to_string(this->GetValue());
        }

        std::string m_valueString;
    };

    template<>
    class CVar<bool> : public detail::CVarCommon<bool>
    {
    public:
        explicit CVar(const std::string& name, const bool value, CVarFlag flags, const std::string& description) :
            detail::CVarCommon<bool>(name, value, flags, description)
        {
        }

        [[nodiscard]] const std::string& GetValueAsString() const
        {
            static const std::string boolStrings[] = { "false", "true" };
            return GetValue() ? boolStrings[0] : boolStrings[1];
        }

        [[nodiscard]] const std::string& GetTypeAsString() const override
        {
            static const std::string typeString = "boolean";
            return typeString;
        }
    };

    template<>
    class CVar<std::string> : public detail::CVarCommon<std::string>
    {
    public:
        explicit CVar(const std::string& name, const std::string& value, CVarFlag flags, const std::string& description) :
            detail::CVarCommon<std::string>(name, value, flags, description)
        {
        }

        [[nodiscard]] const std::string& GetValueAsString() const
        {
            return GetValue();
        }

        [[nodiscard]] const std::string& GetTypeAsString() const override
        {
            static const std::string typeString = "string";
            return typeString;
        }
    };

    using RegistryMapType = std::unordered_map<uint64_t, detail::CVarBase*>;

    /**
     * @brief Register all globally defined cvars to the registry. 
     *        This needs be called once at initialization!
     */
    void RegisterStaticCVars();

    /**
     * @brief Get a const map of the registry containing all registrated cvars.
     */
    [[nodiscard]] const RegistryMapType& GetCVarRegistry();

    /**
     * @brief Try to find a registrated cvar of type T.
     * 
     * Example usage:
     *  auto* cvar = Find(hash::String("cvarName"));
     * 
     * @param hash Hashed value of the cvar's name.
     * @return A pointer to the cvar if it was found and types are matching, nullptr otherwise.
     */
    template<typename T>
    [[nodiscard]] CVar<T>* FindCVar(const uint64_t hash)
    {
        auto it = GetCVarRegistry().find(hash);
        if (it == GetCVarRegistry().end())
            return nullptr;

        detail::CVarBase* cvar = it->second;
        if (cvar->GetType() != typeid(T))
            return nullptr;

        return static_cast<CVar<T>*>(cvar);
    }
}