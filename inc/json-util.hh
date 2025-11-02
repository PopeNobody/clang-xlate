namespace json {
  // === SMART OVERLOAD: json::parse(json) ===
  inline json parse(const json& j)
  {
    if (j.is_string()) {
      return json::parse(j.get<std::string>());
    }
    throw std::invalid_argument(
        "json::parse(json): expected a JSON string, but got " +
        std::string(j.type_name()) +
        ". Did you mean to pass tool_call[\"function\"][\"arguments\"]?"
        );
  }
};
