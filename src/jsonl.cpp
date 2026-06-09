#include "jsonl.hpp"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include <utility>

namespace yllama {
namespace {

struct JsonValue {
  enum class Type { Null, Bool, String, Number, Object, Array };

  Type type = Type::Null;
  bool boolean = false;
  std::string string;
  double number = 0;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
};

class JsonParser {
 public:
  explicit JsonParser(std::string_view input) : input_(input) {}

  bool parse(JsonValue& value, std::string& error) {
    skip_space();
    if (!parse_value(value, error)) {
      return false;
    }
    skip_space();
    if (pos_ != input_.size()) {
      error = "unexpected trailing data";
      return false;
    }
    return true;
  }

 private:
  bool parse_value(JsonValue& value, std::string& error) {
    if (pos_ >= input_.size()) {
      error = "unexpected end of input";
      return false;
    }

    const char ch = input_[pos_];
    if (ch == '"') {
      value.type = JsonValue::Type::String;
      return parse_string(value.string, error);
    }
    if (ch == '{') {
      return parse_object(value, error);
    }
    if (ch == '[') {
      return parse_array(value, error);
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
      value.type = JsonValue::Type::Number;
      return parse_number(value.number, error);
    }
    if (consume_literal("true")) {
      value.type = JsonValue::Type::Bool;
      value.boolean = true;
      return true;
    }
    if (consume_literal("false")) {
      value.type = JsonValue::Type::Bool;
      value.boolean = false;
      return true;
    }
    if (consume_literal("null")) {
      value.type = JsonValue::Type::Null;
      return true;
    }

    error = "unsupported JSON value";
    return false;
  }

  bool parse_object(JsonValue& value, std::string& error) {
    value.type = JsonValue::Type::Object;
    ++pos_;
    skip_space();
    if (consume('}')) {
      return true;
    }

    while (true) {
      std::string key;
      if (!parse_string(key, error)) {
        return false;
      }
      skip_space();
      if (!consume(':')) {
        error = "expected ':'";
        return false;
      }
      skip_space();

      JsonValue field;
      if (!parse_value(field, error)) {
        return false;
      }
      value.object.emplace(std::move(key), std::move(field));

      skip_space();
      if (consume('}')) {
        return true;
      }
      if (!consume(',')) {
        error = "expected ',' or '}'";
        return false;
      }
      skip_space();
    }
  }

  bool parse_array(JsonValue& value, std::string& error) {
    value.type = JsonValue::Type::Array;
    ++pos_;
    skip_space();
    if (consume(']')) {
      return true;
    }

    while (true) {
      JsonValue item;
      if (!parse_value(item, error)) {
        return false;
      }
      value.array.push_back(std::move(item));

      skip_space();
      if (consume(']')) {
        return true;
      }
      if (!consume(',')) {
        error = "expected ',' or ']'";
        return false;
      }
      skip_space();
    }
  }

  bool parse_string(std::string& value, std::string& error) {
    if (!consume('"')) {
      error = "expected string";
      return false;
    }

    std::ostringstream out;
    while (pos_ < input_.size()) {
      const char ch = input_[pos_++];
      if (ch == '"') {
        value = out.str();
        return true;
      }
      if (ch == '\\') {
        if (pos_ >= input_.size()) {
          error = "unterminated escape";
          return false;
        }
        const char escaped = input_[pos_++];
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            out << escaped;
            break;
          case 'b':
            out << '\b';
            break;
          case 'f':
            out << '\f';
            break;
          case 'n':
            out << '\n';
            break;
          case 'r':
            out << '\r';
            break;
          case 't':
            out << '\t';
            break;
          default:
            error = "unsupported string escape";
            return false;
        }
      } else {
        out << ch;
      }
    }

    error = "unterminated string";
    return false;
  }

  bool parse_number(double& value, std::string& error) {
    const std::size_t start = pos_;
    if (input_[pos_] == '-') {
      ++pos_;
    }
    if (pos_ >= input_.size() ||
        !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
      error = "invalid number";
      return false;
    }
    while (pos_ < input_.size() &&
           std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
    if (pos_ < input_.size() && input_[pos_] == '.') {
      ++pos_;
      if (pos_ >= input_.size() ||
          !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
        error = "invalid number";
        return false;
      }
      while (pos_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
      }
    }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) {
        ++pos_;
      }
      if (pos_ >= input_.size() ||
          !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
        error = "invalid number";
        return false;
      }
      while (pos_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
      }
    }

    const std::string number(input_.substr(start, pos_ - start));
    char* end = nullptr;
    value = std::strtod(number.c_str(), &end);
    if (end == number.c_str() || *end != '\0') {
      error = "invalid number";
      return false;
    }
    return true;
  }

  void skip_space() {
    while (pos_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
  }

  bool consume(char expected) {
    if (pos_ < input_.size() && input_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool consume_literal(std::string_view literal) {
    if (input_.substr(pos_, literal.size()) == literal) {
      pos_ += literal.size();
      return true;
    }
    return false;
  }

  std::string_view input_;
  std::size_t pos_ = 0;
};

ParseResult make_error(std::string code, std::string message) {
  return ParseResult{std::nullopt, ParseError{std::move(code), std::move(message)}};
}

const JsonValue* field(const JsonValue& object, std::string_view name) {
  if (object.type != JsonValue::Type::Object) {
    return nullptr;
  }
  const auto iter = object.object.find(std::string(name));
  if (iter == object.object.end()) {
    return nullptr;
  }
  return &iter->second;
}

std::optional<std::string> string_field(const JsonValue& object,
                                        std::string_view name) {
  const JsonValue* value = field(object, name);
  if (value == nullptr || value->type != JsonValue::Type::String) {
    return std::nullopt;
  }
  return value->string;
}

std::optional<int> int_field(const JsonValue& object, std::string_view name) {
  const JsonValue* value = field(object, name);
  if (value == nullptr || value->type != JsonValue::Type::Number) {
    return std::nullopt;
  }
  const double truncated = std::trunc(value->number);
  if (truncated != value->number) {
    return std::nullopt;
  }
  return static_cast<int>(truncated);
}

std::optional<double> number_field(const JsonValue& object,
                                   std::string_view name) {
  const JsonValue* value = field(object, name);
  if (value == nullptr || value->type != JsonValue::Type::Number) {
    return std::nullopt;
  }
  return value->number;
}

std::optional<bool> bool_field(const JsonValue& object, std::string_view name) {
  const JsonValue* value = field(object, name);
  if (value == nullptr || value->type != JsonValue::Type::Bool) {
    return std::nullopt;
  }
  return value->boolean;
}

ParseResult parse_configure(const JsonValue& root) {
  auto id = string_field(root, "id");
  auto model_path = string_field(root, "model_path");
  auto context_tokens = int_field(root, "context_tokens");
  auto threads = int_field(root, "threads");

  if (!id) {
    return make_error("invalid_command", "configure command requires string id");
  }
  if (!model_path) {
    return make_error("invalid_command",
                      "configure command requires string model_path");
  }
  if (!context_tokens) {
    return make_error("invalid_command",
                      "configure command requires numeric context_tokens");
  }
  if (!threads) {
    return make_error("invalid_command", "configure command requires numeric threads");
  }

  return ParseResult{
      ConfigureCommand{*id, *model_path, *context_tokens, *threads}, std::nullopt};
}

ParseResult parse_generate(const JsonValue& root) {
  auto id = string_field(root, "id");
  if (!id) {
    return make_error("invalid_command", "generate command requires string id");
  }

  const JsonValue* input = field(root, "input");
  if (input == nullptr || input->type != JsonValue::Type::Object) {
    return make_error("invalid_command", "generate command requires input object");
  }

  auto kind = string_field(*input, "kind");
  if (!kind) {
    return make_error("invalid_command", "generate input requires string kind");
  }

  GenerateInput generate_input;
  if (*kind == "prompt") {
    auto prompt = string_field(*input, "prompt");
    if (!prompt) {
      return make_error("invalid_command", "prompt input requires string prompt");
    }
    generate_input = PromptInput{*prompt};
  } else if (*kind == "messages") {
    const JsonValue* messages = field(*input, "messages");
    if (messages == nullptr || messages->type != JsonValue::Type::Array) {
      return make_error("invalid_command", "messages input requires messages array");
    }

    MessagesInput messages_input;
    for (const JsonValue& message : messages->array) {
      if (message.type != JsonValue::Type::Object) {
        return make_error("invalid_command", "message item must be an object");
      }
      auto role = string_field(message, "role");
      auto content = string_field(message, "content");
      if (!role || !content) {
        return make_error("invalid_command",
                          "message item requires string role and content");
      }
      messages_input.messages.push_back(Message{*role, *content});
    }
    generate_input = std::move(messages_input);
  } else {
    return make_error("invalid_command", "unsupported generate input kind");
  }

  GenerateSettings settings;
  if (const JsonValue* value = field(root, "settings"); value != nullptr) {
    if (value->type != JsonValue::Type::Object) {
      return make_error("invalid_command", "generate settings must be an object");
    }

    settings.temperature = number_field(*value, "temperature");
    settings.top_p = number_field(*value, "top_p");
    settings.max_tokens = int_field(*value, "max_tokens");
    settings.stream = bool_field(*value, "stream");

    if (field(*value, "max_tokens") != nullptr && !settings.max_tokens) {
      return make_error("invalid_command", "max_tokens must be an integer");
    }
    if (field(*value, "stream") != nullptr && !settings.stream) {
      return make_error("invalid_command", "stream must be a boolean");
    }

    if (const JsonValue* stop = field(*value, "stop"); stop != nullptr) {
      if (stop->type != JsonValue::Type::Array) {
        return make_error("invalid_command", "stop must be an array");
      }
      for (const JsonValue& item : stop->array) {
        if (item.type != JsonValue::Type::String) {
          return make_error("invalid_command", "stop entries must be strings");
        }
        settings.stop.push_back(item.string);
      }
    }
  }

  return ParseResult{GenerateCommand{*id, std::move(generate_input), settings},
                     std::nullopt};
}

ParseResult parse_cancel(const JsonValue& root) {
  auto id = string_field(root, "id");
  if (!id) {
    return make_error("invalid_command", "cancel command requires string id");
  }
  return ParseResult{CancelCommand{*id}, std::nullopt};
}

ParseResult parse_shutdown(const JsonValue& root) {
  auto id = string_field(root, "id");
  if (!id) {
    return make_error("invalid_command", "shutdown command requires string id");
  }
  return ParseResult{ShutdownCommand{*id}, std::nullopt};
}

}  // namespace

ParseResult parse_command_line(std::string_view line) {
  JsonValue root;
  std::string parse_error;
  JsonParser parser(line);
  if (!parser.parse(root, parse_error)) {
    return make_error("invalid_json", parse_error);
  }
  if (root.type != JsonValue::Type::Object) {
    return make_error("invalid_command", "command must be a JSON object");
  }

  auto type = string_field(root, "type");
  if (!type) {
    return make_error("invalid_command", "command requires string type");
  }

  if (*type == "configure") {
    return parse_configure(root);
  }
  if (*type == "generate") {
    return parse_generate(root);
  }
  if (*type == "cancel") {
    return parse_cancel(root);
  }
  if (*type == "shutdown") {
    return parse_shutdown(root);
  }

  return make_error("unknown_command", "unknown command type");
}

}  // namespace yllama
