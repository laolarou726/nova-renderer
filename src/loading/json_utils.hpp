#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "nova_renderer/util/result.hpp"

#include "../util/logger.hpp"

namespace nova::renderer {
    // Keeps the compiler happy
    std::string to_string(const std::string& str);

    /*!
     * \brief Retrieves an individual value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve
     * \param json_obj The JSON object where your value might be found
     * \param key The name of the value
     * \return An optional that contains the value, if it can be found, or an empty optional if the value cannot be found
     */
    template <typename ValType, std::enable_if_t<!std::is_same_v<ValType, std::string>>** = nullptr>
    std::optional<ValType> get_json_value(const nlohmann::json& json_obj, const std::string& key) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            return std::optional<ValType>(json_node.get<ValType>());
        }

        return std::optional<ValType>{};
    }

    /*!
     * \brief Retrieves an individual string value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve (always std::string here)
     * \param json_obj The JSON object where your string might be found
     * \param key The name of the string
     * \param empty_means_not_present If set to true an empty string will be interpreted as not found
     * \return An optional that contains the value, if it can be found, or an empty optional if the value cannot be found
     */
    template <typename ValType, std::enable_if_t<std::is_same_v<ValType, std::string>>** = nullptr>
    std::optional<ValType> get_json_value(const nlohmann::json& json_obj, const std::string& key, bool empty_means_not_present = false) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto str = json_obj.at(key).get<std::string>();
            return (empty_means_not_present && str.empty()) ? std::optional<std::string>{} : std::optional<std::string>(str);
        }

        return std::optional<std::string>{};
    }

    /*!
     * \brief Retrieves an individual value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve
     * \param json_obj The JSON object where your value might be found
     * \param key The name of the value
     * \param default_value The value to use if the requested key isn't present in the JSON
     * \return The value from the JSON if the key exists in the JSON, or `default_value` if it does not
     */
    template <typename ValType>
    ValType get_json_value(const nlohmann::json& json_obj, const std::string& key, ValType default_value) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            return json_node.get<ValType>();
        }

        NOVA_LOG(DEBUG) << key << " not found - using a default value";
        return default_value;
    }

    /*!
     * \brief Retrieves an individual value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve
     * \param json_obj The JSON object where your value might be found
     * \param key The name of the value
     * \param deserializer A function that deserializes the JSON value
     * \return An optional that contains the value, if it can be found, or an empty optional if the value cannot be found
     */
    template <typename ValType>
    std::optional<ValType> get_json_value(const nlohmann::json& json_obj,
                                          const std::string& key,
                                          std::function<ValType(const nlohmann::json&)> deserializer) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            ValType val = deserializer(json_node);
            return std::optional<ValType>{std::move(val)};
        }

        return std::optional<ValType>{};
    }

    /*!
     * \brief Retrieves an individual value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve
     * \param json_obj The JSON object where your value might be found
     * \param key The name of the value
     * \param deserializer A function that deserializes the JSON value
     * \return An optional that contains the value, if it can be found, or an empty optional if the value cannot be found
     */
    template <typename ValType>
    result<ValType> get_json_value(const nlohmann::json& json_obj,
                                   const std::string& key,
                                   std::function<result<ValType>(const nlohmann::json&)> deserializer) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            return deserializer(json_node);
        }

        return MAKE_ERROR("Json key {:s} not found", key);
    }

    /*!
     * \brief Retrieves an individual value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve
     * \param json_obj The JSON object where your value might be found
     * \param key The name of the value
     * \param default_value The value to use if the requested key isn't present in the JSON
     * \param deserializer A function that deserializes the JSON value
     * \return The value from the JSON if the key exists in the JSON, or `default_value` if it does not
     */
    template <typename ValType>
    ValType get_json_value(const nlohmann::json& json_obj,
                           const std::string& key,
                           ValType default_value,
                           std::function<ValType(const nlohmann::json&)> deserializer) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            ValType value = deserializer(json_node);
            return value;
        }

        using std::to_string;
        NOVA_LOG(DEBUG) << key << " not found - defaulting to " << to_string(default_value);

        return default_value;
    }

    /*!
     * \brief Retrieves an individual value from the provided JSON structure
     * \tparam ValType The type of the value to retrieve
     * \param json_obj The JSON object where your value might be found
     * \param key The name of the value
     * \param default_value The value to use if the requested key isn't present in the JSON
     * \param deserializer A function that deserializes the JSON value into a result
     * \return The value from the JSON if the key exists in the JSON, or `default_value` if it does not
     */
    template <typename ValType>
    ValType get_json_value(const nlohmann::json& json_obj,
                           const std::string& key,
                           ValType default_value,
                           std::function<result<ValType>(const nlohmann::json&)> deserializer) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            result<ValType> result = deserializer(json_node);
            if(result) {
                return result.value;
            }
        }

        using std::to_string;
        NOVA_LOG(DEBUG) << key << " not found - defaulting to " << to_string(default_value);

        return default_value;
    }

    /*!
     * \brief Retrieves an array of values from the provided JSON object
     * \tparam ValType The type fo the values in the array
     * \param json_obj The JSON object where the values might be found
     * \param key The name fo the array that has your values
     * \return An array of values, if the value can be found, or an empty vector if the values cannot be found
     */
    template <typename ValType>
    std::vector<ValType> get_json_array(const nlohmann::json& json_obj, const std::string& key) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            std::vector<ValType> vec;
            vec.reserve(json_node.size());

            for(auto& elem : json_node) {
                vec.push_back(elem.get<ValType>());
            }

            return vec;
        }

        return std::vector<ValType>{};
    }

    /*!
     * \brief Retrieves an array of values from the provided JSON object
     * \tparam ValType The type fo the values in the array
     * \param json_obj The JSON object where the values might be found
     * \param key The name fo the array that has your values
     * \param deserializer A function that can deserialize each value from JSON
     * \return An array of values, if the value can be found, or an empty vector if the values cannot be found
     */
    template <typename ValType>
    std::vector<ValType> get_json_array(const nlohmann::json& json_obj,
                                        const std::string& key,
                                        std::function<ValType(const nlohmann::json&)> deserializer) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            std::vector<ValType> vec;
            vec.reserve(json_node.size());

            for(auto& elem : json_node) {
                vec.push_back(deserializer(elem));
            }

            return vec;
        }

        return std::vector<ValType>{};
    }

    /*!
     * \brief Retrieves an array of values from the provided JSON object
     * \tparam ValType The type fo the values in the array
     * \param json_obj The JSON object where the values might be found
     * \param key The name fo the array that has your values
     * \param deserializer A function that can deserialize each value from JSON
     * \return An array of values, if the value can be found, or an empty vector if the values cannot be found
     */
    template <typename ValType>
    result<std::vector<ValType>> get_json_array(const nlohmann::json& json_obj,
                                                const std::string& key,
                                                std::function<result<ValType>(const nlohmann::json&)> deserializer) {
        const auto& itr = json_obj.find(key);
        if(itr != json_obj.end()) {
            auto& json_node = json_obj.at(key);
            std::vector<ValType> vec;
            vec.reserve(json_node.size());

            for(auto& elem : json_node) {
                result<ValType> deserialization_result = deserializer(elem);
                if(!deserialization_result) {
                    return deserialization_result.template convert<std::vector<ValType>>();
                }

                vec.push_back(deserialization_result.value);
            }

            return vec;
        }

        return MAKE_ERROR("Json key {:s} not found", key);
    }
} // namespace nova::renderer
