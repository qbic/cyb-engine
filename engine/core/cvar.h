#pragma once
#include <algorithm>
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
        SystemBit = BIT(0),         ///< CVar belongs to system.
        RendererBit = BIT(1),       ///< CVar belongs to renderer.
        GuiBit      = BIT(2),       ///< CVar belongs to GUI.
        GameBit     = BIT(3),       ///< CVar belongs to game.
        RomBit      = BIT(10),      ///< Read only access, cannot be changed by user.
        NoSaveBit   = BIT(11),      ///< Cvar wont be written during serialization.
        ModifiedBit = BIT(12)       ///< Flags that CVar has been modified since init.
    };
    CYB_ENABLE_BITMASK_OPERATORS(CVarFlag);

    /** Undefined so that only explicit types of CVar is allowed. */
    template <typename T>
    class CVar;

    /** CVar base functionality that doesn't require value type. */
    class CVarBase : private NonCopyable
    {
    public:
        explicit CVarBase(const std::string& name, CVarFlag flags, const std::string& description);
        virtual ~CVarBase() = default;

        [[nodiscard]] uint64_t GetHash() const;
        [[nodiscard]] const std::string& GetName() const;
        [[nodiscard]] const std::string& GetDescription() const;
        [[nodiscard]] bool IsModified() const;
        void SetModified(bool value);
        [[nodiscard]] bool IsReadOnly() const;

        [[nodiscard]] virtual const std::type_info& GetType() const = 0;
        [[nodiscard]] virtual const std::string& GetValueAsString() const = 0;
        [[nodiscard]] virtual const std::string_view GetTypeAsString() const = 0;

    private:
        const std::string m_name;
        const std::string m_description;
        const uint64_t m_hash;                  // Computed from name
        CVarFlag m_flags;
    };

    /** CVar common functionality that is general for all value types. */
    template <typename T>
    class CVarCommon : public CVarBase
    {
    public:
        using CallbackType = std::function<void(const CVar<T>&)>;

        explicit CVarCommon(const std::string& name, T value, CVarFlag flags, const std::string& description) :
            CVarBase(name, flags, description),
            m_value(value)
        {
        }
        virtual ~CVarCommon() = default;

        void SetValue(const T& value)
        {
            if (IsReadOnly() || m_value == value)
                return;

            m_value = value;
            OnModifyValue();
            SetModified(true);
            RunOnChangeCallbacks();
        }

        [[nodiscard]] const std::type_info& GetType() const override final { return typeid(T); }
        [[nodiscard]] const T& GetValue() const { return m_value; }
        [[nodiscard]] const std::string_view GetTypeAsString() const override final { return typeid(T).name(); }

        void RunOnChangeCallbacks() const
        {
            for (auto& callback : m_callbacks)
                callback(*static_cast<const CVar<T>*>(this));
        }

        void ClearCallbacks()
        {
            m_callbacks.clear();
        }

        void RegisterOnChangeCallback(const CallbackType callback)
        {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        virtual void OnModifyValue() {}

        T m_value;
        std::vector<CallbackType> m_callbacks;
    };

    // Every supported type of CVar must be explicitly specialized. 
    // This concept is used to restrict the template parameter of CVar to only supported types.
    template <typename T>
    concept cvar_number_value = 
        std::same_as<T, int32_t>  ||
        std::same_as<T, uint32_t> ||
        std::same_as<T, float>;

    /** CVar specialization for numerical types with optional min and max values. */
    template <cvar_number_value T>
    class CVar<T> : public CVarCommon<T>
    {
    public:
        explicit CVar(const std::string& name, const T value, const T minValue, const T maxValue, CVarFlag flags, const std::string& description) :
            CVarCommon<T>(name, value, flags, description),
            m_minValue(minValue),
            m_maxValue(maxValue)
        {
            OnModifyValue();
        }

        explicit CVar(const std::string& name, const T value, CVarFlag flags, const std::string& description) :
            CVar(name, value, std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), flags, description)
        {
        }

        virtual ~CVar() = default;

        [[nodiscard]] const std::string& GetValueAsString() const
        {
            return m_valueAsString;
        }

    private:
        void OnModifyValue() override
        {
            const T clampedValue = std::clamp(this->GetValue(), m_minValue, m_maxValue);
            if (clampedValue != this->GetValue())
            {
                // Return here, SetValue() will call this function again.
                this->SetValue(clampedValue);
                return;
            }

            if constexpr (std::is_same_v<T, float>)
                m_valueAsString = std::format("{:.2f}", this->GetValue());
            else
                m_valueAsString = std::to_string(this->GetValue());
        }

    private:
        T m_minValue;
        T m_maxValue;
        std::string m_valueAsString;    // Cached string representation of the value
    };

    /** CVar specialization for bool. */
    template <>
    class CVar<bool> : public CVarCommon<bool>
    {
    public:
        explicit CVar(const std::string& name, const bool value, CVarFlag flags, const std::string& description) :
            CVarCommon<bool>(name, value, flags, description)
        {
        }

        virtual ~CVar() = default;

        [[nodiscard]] const std::string& GetValueAsString() const override
        {
            static const std::string boolStrings[] = { "false", "true" };
            return GetValue() ? boolStrings[1] : boolStrings[0];
        }
    };

    /** CVar specialization for std::string. */
    template <>
    class CVar<std::string> : public CVarCommon<std::string>
    {
    public:
        explicit CVar(const std::string& name, const std::string& value, CVarFlag flags, const std::string& description) :
            CVarCommon<std::string>(name, value, flags, description)
        {
        }

        virtual ~CVar() = default;

        [[nodiscard]] const std::string& GetValueAsString() const override
        {
            return GetValue();
        }
    };

    using CVarRegistryMapType = std::unordered_map<uint64_t, CVarBase*>;

    /** 
     * Register all globally defined cvars to the registry.
     * This needs be called once at initialization!
     */
    void RegisterStaticCVars();

    /** Get a const map of the registry containing all registered cvars. */
    [[nodiscard]] const CVarRegistryMapType& GetCVarRegistry();

    /**
     * Try to find a registered cvar of type T.
     *
     * Example usage:
     *  auto* cvar = Find<bool>(hash::String("cvarName"));
     *
     * @param hash Hashed value of the cvar's name.
     * @return A pointer to the cvar if it was found and types are matching, nullptr otherwise.
     */
    template <typename T>
    [[nodiscard]] CVar<T>* FindCVar(const uint64_t hash)
    {
        auto it = GetCVarRegistry().find(hash);
        if (it == GetCVarRegistry().end())
            return nullptr;

        CVarBase* cvar = it->second;
        if (cvar->GetType() != typeid(T))
            return nullptr;

        return static_cast<CVar<T>*>(cvar);
    }
} // namespace cyb