#pragma once
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
        SystemBit   = BIT(0),       //!< CVar belongs to core system.
        RendererBit = BIT(1),       //!< CVar belongs to renderer.
        GuiBit      = BIT(2),       //!< CVar belongs to GUI.
        GameBit     = BIT(3),       //!< CVar belongs to game.
        RomBit      = BIT(10),      //!< Read only access, cannot be changed by user.
        NoSaveBit   = BIT(11),      //!< Cvar wont be written during serialization.
        ModifiedBit = BIT(12)       //!< Flags that CVar has been modified sence init.
    };
    CYB_ENABLE_BITMASK_OPERATORS(CVarFlag);

    /**
     * @brief Undefined to that only defined types of CVar is allowed.
     */
    template <typename T>
    class CVar;

    namespace detail
    {
        /**
         * @brief CVar base functionality that doesen't require value type.
         */
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
            const uint64_t m_hash;              // computed from name
            CVarFlag m_flags;
        };

        /**
         * @brief CVar common functionality that is general for all value types.
         */
        template<typename T>
        class CVarCommon : public CVarBase
        {
        public:
            using ValueType    = T;
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

            [[nodiscard]] const std::string& GetTypeAsString() const override
            {
                static const std::string typeStr = typeid(T).name();
                return typeStr;
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
            std::vector<CallbackType> m_callbacks;
        };
    }

    template<typename T>
    concept IsNumberFamily = std::same_as<T, int32_t> ||
                             std::same_as<T, uint32_t> ||
                             std::same_as<T, float>;

    /**
     * @brief CVar of number family type implementation.
     */
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

    /**
     * @brief CVar of boolean type implementation.
     */
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
            return GetValue() ? boolStrings[1] : boolStrings[0];
        }
    };

    /**
     * @brief CVar of string type implementation.
     */
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
    };

    using CVarRegistryMapType = std::unordered_map<uint64_t, detail::CVarBase*>;

    /**
     * @brief Register all globally defined cvars to the registry. 
     *        This needs be called once at initialization!
     */
    void RegisterStaticCVars();

    /**
     * @brief Get a const map of the registry containing all registrated cvars.
     */
    [[nodiscard]] const CVarRegistryMapType& GetCVarRegistry();

    /**
     * @brief Try to find a registrated cvar of type T.
     * 
     * Example usage:
     *  auto* cvar = Find<bool>(hash::String("cvarName"));
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