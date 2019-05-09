#include "nlohmann/json.hpp"
#include <fstream>
#include <unordered_set>

template <typename T>
struct BoxedOptional {

    BoxedOptional() = default;

    BoxedOptional(T value): value{new T{value}} {}

    BoxedOptional(BoxedOptional const & optional) {
        copy_value_from(optional);
    }

    BoxedOptional & operator = (BoxedOptional const & optional) {
        reset();
        copy_value_from(optional);
        return *this;
    }

    BoxedOptional(BoxedOptional && optional) {
        steal_value_from(optional);
    }

    BoxedOptional & operator = (BoxedOptional && optional) {
        reset();
        steal_value_from(optional);
        return *this;
    }

    ~BoxedOptional() {
        reset();
    }

    void reset() {
        if (value) {
            delete value;
            value = nullptr;
        }
    }

    bool has_value() const {
        return value != nullptr;
    }

    explicit operator bool() const {
        return has_value();
    }

    T & operator * () {
        return *value;
    }

    T const & operator * () const {
        return *value;
    }

    T * operator -> () {
        return value;
    }

    T const * operator -> () const {
        return value;
    }

private:
    T * value = nullptr;

    void copy_value_from(BoxedOptional const & optional) {
        if (optional.value) {
            value = new T{*optional.value};
        }
    }

    void steal_value_from(BoxedOptional & optional) {
        if (optional.value) {
            value = optional.value;
            optional.value = nullptr;
        }
    }

};

struct Schema {

    struct Type {

        enum class Kind: uint8_t {
            Scalar, Object, Interface, Union, Enum, InputObject, List, NonNull
        };

        enum class Scalar: uint8_t {
            // 32 bit
            Int,
            // Double
            Float,
            // UTF-8
            String,
            Boolean,
            ID
        };

        struct TypeRef {
            Kind kind;
            std::optional<std::string> name;
            // NonNull and List only
            BoxedOptional<TypeRef> ofType;

            TypeRef const & underlyingType() const {
                if (ofType) {
                    return ofType->underlyingType();
                }
                return *this;
            }

        };

        struct InputValue {
            std::string name;
            std::string description;
            TypeRef type;
            // TODO: Default value
        };

        struct Field {
            std::string name;
            std::string description;
            std::vector<InputValue> args;
            TypeRef type;
            // TODO: Deprecation
        };

        struct EnumValue {
            std::string name;
            std::string description;
            // TODO: Deprecation
        };

        Kind kind;
        std::string name;
        std::string description;
        // Object and Interface only
        std::vector<Field> fields;
        // InputObject only
        std::vector<InputValue> inputFields;
        // Object only
        std::vector<TypeRef> interfaces;
        // Enum only
        std::vector<EnumValue> enumValues;
        // Interface and Union only
        std::vector<TypeRef> possibleTypes;
    };

    struct TypeName {
        std::string name;
    };

    std::optional<TypeName> queryType;
    std::optional<TypeName> mutationType;
    std::optional<TypeName> subscriptionType;
    std::vector<Type> types;
    // TODO: Directives
};

namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json & json, std::optional<T> const & opt) {
            if (opt.has_value()) {
                json = *opt;
            } else {
                json = nullptr;
            }
        }

        static void from_json(const json & json, std::optional<T> & opt) {
            if (json.is_null()) {
                opt.reset();
            } else {
                opt = json.get<T>();
            }
        }
    };

    template <typename T>
    struct adl_serializer<BoxedOptional<T>> {
        static void to_json(json & json, BoxedOptional<T> const & opt) {
            if (opt.has_value()) {
                json = *opt;
            } else {
                json = nullptr;
            }
        }

        static void from_json(const json & json, BoxedOptional<T> & opt) {
            if (json.is_null()) {
                opt.reset();
            } else {
                opt = json.get<T>();
            }
        }
    };
}

using Json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(Schema::Type::Kind, {
    {Schema::Type::Kind::Scalar, "SCALAR"},
    {Schema::Type::Kind::Object, "OBJECT"},
    {Schema::Type::Kind::Interface, "INTERFACE"},
    {Schema::Type::Kind::Union, "UNION"},
    {Schema::Type::Kind::Enum, "ENUM"},
    {Schema::Type::Kind::InputObject, "INPUT_OBJECT"},
    {Schema::Type::Kind::List, "LIST"},
    {Schema::Type::Kind::NonNull, "NON_NULL"}
});

template <typename T>
inline void get_value_to(Json const & json, char const * key, T & target) {
    json.at(key).get_to(target);
}

template <typename T>
inline void get_value_to(Json const & json, char const * key, std::optional<T> & target) {
    auto it = json.find(key);
    if (it != json.end()) {
        it->get_to(target);
    } else {
        target.reset();
    }
}

template <typename T>
inline void get_value_to(Json const & json, char const * key, BoxedOptional<T> & target) {
    auto it = json.find(key);
    if (it != json.end()) {
        it->get_to(target);
    } else {
        target.reset();
    }
}

template <typename T>
inline void set_value_from(Json & json, char const * key, T const & source) {
    json[key] = source;
}

template <typename T>
inline void set_value_from(Json & json, char const * key, std::optional<T> const & source) {
    if (source) {
        json[key] = *source;
        return;
    }
    auto it = json.find(key);
    if (it != json.end()) {
        json.erase(it);
    }
}

void from_json(Json const & json, Schema::Type::TypeRef & type) {
    get_value_to(json, "kind", type.kind);
    get_value_to(json, "name", type.name);
    get_value_to(json, "ofType", type.ofType);
}

void from_json(Json const & json, Schema::Type::InputValue & input) {
    get_value_to(json, "name", input.name);
    get_value_to(json, "description", input.description);
    get_value_to(json, "type", input.type);
}

void from_json(Json const & json, Schema::Type::Field & field) {
    get_value_to(json, "name", field.name);
    get_value_to(json, "description", field.description);
    get_value_to(json, "args", field.args);
    get_value_to(json, "type", field.type);
}

void from_json(Json const & json, Schema::Type::EnumValue & value) {
    get_value_to(json, "name", value.name);
    get_value_to(json, "description", value.description);
}

void from_json(Json const & json, Schema::Type & type) {
    get_value_to(json, "kind", type.kind);
    get_value_to(json, "name", type.name);
    get_value_to(json, "description", type.description);
    get_value_to(json, "fields", type.fields);
    get_value_to(json, "inputFields", type.inputFields);
    get_value_to(json, "interfaces", type.interfaces);
    get_value_to(json, "enumValues", type.enumValues);
    get_value_to(json, "possibleTypes", type.possibleTypes);
}

void from_json(Json const & json, Schema::TypeName & typeName) {
    get_value_to(json, "name", typeName.name);
}

void from_json(Json const & json, Schema & schema) {
    get_value_to(json, "queryType", schema.queryType);
    get_value_to(json, "mutationType", schema.mutationType);
    get_value_to(json, "subscriptionType", schema.subscriptionType);
    get_value_to(json, "types", schema.types);
}

constexpr size_t spacesPerIndent = 4;
const std::string unknownEnumCase = "Unknown";

std::string indent(size_t indentation) {
    return std::string(indentation * spacesPerIndent, ' ');
}

void appendDescription(std::string & string, std::string const & description, size_t indentation) {
    if (description.empty()) {
        return;
    }
    string += indent(indentation) + "// " + description + "\n";
}

std::string screamingSnakeCaseToPascalCase(std::string const & snake) {
    std::string pascal;

    bool isFirstInWord = true;
    for (auto const & character : snake) {
        if (character == '_') {
            isFirstInWord = true;
            continue;
        }

        if (isFirstInWord) {
            pascal += toupper(character);
            isFirstInWord = false;
        } else {
            pascal += tolower(character);
        }
    }

    return pascal;
}

std::vector<Schema::Type> sortCustomTypesByDependencyOrder(std::vector<Schema::Type> const &types) {
    using namespace std;

    struct TypeWithDependencies {
        Schema::Type type;
        unordered_set<string> dependencies;
    };

    unordered_map<string, unordered_set<string>> typesToDependents;
    unordered_map<string, TypeWithDependencies> typesToDependencies;

    auto isCustomType = [](Schema::Type::Kind kind) {
        switch (kind) {
            case Schema::Type::Kind::Object:
            case Schema::Type::Kind::Interface:
            case Schema::Type::Kind::Union:
            case Schema::Type::Kind::Enum:
            case Schema::Type::Kind::InputObject:
                return true;

            case Schema::Type::Kind::Scalar:
            case Schema::Type::Kind::List:
            case Schema::Type::Kind::NonNull:
                return false;
        }
    };

    for (auto const & type : types) {
        if (!isCustomType(type.kind)) {
            continue;
        }

        unordered_set<string> dependencies;

        auto addDependency = [&](auto const &dependency) {
            if (dependency.name && isCustomType(dependency.kind)) {
                typesToDependents[*dependency.name].insert(type.name);
                dependencies.insert(*dependency.name);
            }
        };

        for (auto const & field : type.fields) {
            addDependency(field.type.underlyingType());

            for (auto const & arg : field.args) {
                addDependency(arg.type.underlyingType());
            }
        }

        for (auto const & field : type.inputFields) {
            addDependency(field.type.underlyingType());
        }

        for (auto const & interface : type.interfaces) {
            addDependency(interface);
        }

        typesToDependencies[type.name] = {type, move(dependencies)};
    }

    vector<Schema::Type> sortedTypes;

    while (!typesToDependencies.empty()) {
        auto const initialCount = typesToDependencies.size();

        vector<string> addedTypeNames;

        for (auto const & pair : typesToDependencies) {
            if (pair.second.dependencies.empty()) {
                sortedTypes.push_back(pair.second.type);
                addedTypeNames.push_back(pair.first);

                for (auto const & dependentName : typesToDependents[pair.first]) {
                    typesToDependencies[dependentName].dependencies.erase(pair.first);
                }
            }
        }

        for (auto const &addedName : addedTypeNames) {
            typesToDependencies.erase(addedName);
        }

        if (typesToDependencies.size() == initialCount) {
            throw runtime_error{"Circular dependencies in schema"};
        }
    }

    return sortedTypes;
}

std::string generateEnum(Schema::Type const & type, size_t indentation) {
    std::string generated;
    generated += indent(indentation) + "enum class " + type.name + " {\n";

    auto const valueIndentation = indentation + 1;

    for (auto const & value : type.enumValues) {
        appendDescription(generated, value.description, valueIndentation);
        generated += indent(valueIndentation) + screamingSnakeCaseToPascalCase(value.name) + ",\n";
    }

    generated += indent(valueIndentation) + unknownEnumCase + " = -1\n";

    generated += indent(indentation) + "};\n\n";

    return generated;
}

std::string generateEnumSerialization(Schema::Type const & type, size_t indentation) {
    std::string generated;

    generated += indent(indentation) + "NLOHMANN_JSON_SERIALIZE_ENUM(" + type.name + ", {\n";

    auto const valueIndentation = indentation + 1;

    generated += indent(valueIndentation) + "{" + type.name + "::" + unknownEnumCase + ", nullptr},\n";

    for (auto const & value : type.enumValues) {
        generated += indent(valueIndentation) + "{" + type.name + "::" + screamingSnakeCaseToPascalCase(value.name) + ", \"" + value.name + "\"},\n";
    }

    generated += indent(indentation) + "});\n\n";

    return generated;
}

std::string generateTypes(Schema const & schema) {
    std::string source;

    auto const sortedTypes = sortCustomTypesByDependencyOrder(schema.types);

    size_t typeIndentation = 0;

    for (auto const & type : sortedTypes) {
        switch (type.kind) {
            case Schema::Type::Kind::Object:

                break;

            case Schema::Type::Kind::Interface:

                break;

            case Schema::Type::Kind::Union:

                break;

            case Schema::Type::Kind::Enum:
                source += generateEnum(type, typeIndentation);
                source += generateEnumSerialization(type, typeIndentation);
                break;

            case Schema::Type::Kind::InputObject:

                break;

            case Schema::Type::Kind::Scalar:
            case Schema::Type::Kind::List:
            case Schema::Type::Kind::NonNull:
                break;
        }
    }

    return source;
}

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        printf("Please provide an input schema\n");
        return 0;
    }

    std::ifstream file{argv[1]};
    auto const json = Json::parse(file);
    Schema schema = json.at("data").at("__schema");

    auto const source = generateTypes(schema);
    std::ofstream out{"Generated.cpp"};
    out << source;
    out.close();

    return 0;
}
